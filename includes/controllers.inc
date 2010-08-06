<?php
// $Id$

/**
 * @file
 * Controller/loader classes. Modelled on the Drupal 7 entity system.
 */

abstract class VersioncontrolEntityController {
  protected $entityType;
  protected $entityCache = array();
  protected $baseTable;
  protected $idKey;
  protected $backends = array();
  protected $options = array();

  /**
   * An array of default options appended to options passed in by the caller.
   * @var array
   */
  public $defaultOptions = array(
    'determine backend' => TRUE,
    'may cache' => TRUE,
    'callback' => NULL,
    'repository' => NULL,
  );

  /**
   * If set, contains an instance of a VersioncontrolBackend object; this object
   * provides meta-information, as well as acting as a factory that takes data
   * retrieved by this controller and instanciating entities.
   *
   * @var VersioncontrolBackend
   */
  protected $backend;

  /**
   * A mapping of shortened strings used as keys to query building methods they
   * should call.
   *
   * @var array
   */
  protected $typeMap = array(
    'repo'      => 'Repository',
    'account'   => 'Account',
    'operation' => 'Operation',
    'item'      => 'Item',
    'branch'    => 'Branch',
    'tag'       => 'Tag',
  );

  public function __construct() {
    $backends = versioncontrol_get_backends();
    if (variable_get('versioncontrol_single_backend_mode', FALSE)) {
      $this->backend = reset($backends);
    }
    else {
      $this->backends = $backends;
    }
  }

  /**
   * Indicate that this controller can safely restrict itself to a single
   * backend type. This results in some logic & query optimization.
   *
   * @param VersioncontrolBackend $backend
   */
  public function setBackend(VersioncontrolBackend $backend) {
    $this->backend = $backend;
  }

  public function resetBackend() {
    $this->backend = NULL;
  }

  public function resetCache() {
    $this->entityCache = array();
  }

  /**
   * Load, instanciate and cache a set of versioncontrol entities, according to
   * specified parameters.
   *
   * This generic parent loader is extended by the entity-specific controllers;
   * in most cases, this outermost method need not be overwritten as loading
   * behavior can be sufficiently altered by overriding submethods.
   *
   * @param array $ids
   *   An array of entity ids that should be loaded.
   * @param array $conditions
   *   Additional conditions that should be attached to the query and/or used to
   *   filter results from the cache.
   * @param array $options
   *   A variable array of additional options, treated differently (or ignored)
   *   by each backend. The only common element is 'callback', which allows
   *   modules to define a callback that can be fired at the very end of the
   *   querybuilding process to perform additional modifications.
   * @return mixed
   */
  public function load($ids = array(), $conditions = array(), $options = array()) {
    $entities = array();
    // Place passed options in a property to make signatures less cumbersome.
    $this->options = $options + $this->defaultOptions;

    // Create a new variable which is either a prepared version of the $ids
    // array for later comparison with the entity cache, or FALSE if no $ids
    // were passed. The $ids array is reduced as items are loaded from cache,
    // and we need to know if it's empty for this reason to avoid querying the
    // database when all requested entities are loaded from cache.
    $passed_ids = !empty($ids) ? array_flip($ids) : FALSE;
    // Try to load entities from the static cache.
    if ($this->options['may cache']) {
      $entities += $this->cacheGet($ids, $conditions);
      // If any entities were loaded, remove them from the ids still to load.
      if ($passed_ids) {
        $ids = array_keys(array_diff_key($passed_ids, $entities));
      }
    }

    // Load any remaining entities from the database. This is the case if $ids
    // is set to FALSE (so we load all entities), if there are any ids left to
    // load, if loading a revision, or if $conditions was passed without $ids.
    if ($ids === FALSE || $ids || ($conditions && !$passed_ids)) {
      // Build the query.
      $query = $this->buildQuery($ids, $conditions);
      $queried_entities = $query->execute()->fetchAllAssoc($this->idKey);
    }

    if (!empty($queried_entities)) {
      $this->extendData($queried_entities);
      $built_entities = $this->buildEntities($queried_entities);
      $entities += $built_entities;
    }

    if ($this->options['may cache'] && !empty($built_entities)) {
      // Add entities to the cache.
      $this->cacheSet($built_entities);
    }

    // Ensure that the returned array is ordered the same as the original
    // $ids array if this was passed in and remove any invalid ids.
    if ($passed_ids) {
      // Remove any invalid ids from the array.
      $passed_ids = array_intersect_key($passed_ids, $entities);
      foreach ($entities as $entity) {
        $passed_ids[$entity->{$this->idKey}] = $entity;
      }
      $entities = $passed_ids;
    }

    // Reset the options property to an empty array.
    $this->options = array();
    return $entities;
  }

  /**
   * Build the query to load the entity.
   *
   * This has full revision support. For entities requiring special queries,
   * the class can be extended, and the default query can be constructed by
   * calling parent::buildQuery(). This is usually necessary when the object
   * being loaded needs to be augmented with additional data from another
   * table, such as loading node type into comments or vocabulary machine name
   * into terms, however it can also support $conditions on different tables.
   * See CommentController::buildQuery() or TaxonomyTermController::buildQuery()
   * for examples.
   *
   * @return SelectQuery
   *   A SelectQuery object for loading the entity.
   */
  protected function buildQuery($ids, $conditions) {
    $query = $this->buildQueryBase($ids, $conditions);
    $this->buildQueryConditions($query, $ids, $conditions);

    // Allow augmentation of the query by the current backend, if available.
    if ($this->backend instanceof VersioncontrolBackend) {
      $this->backend->augmentEntitySelectQuery($query, $this->entityType);
    }
    // Or determine the backend for these entities in the query, unless query
    // options tell us not to.
    else if ($this->options['determine backend'] === TRUE) {
      $this->queryAlterGetBackendType($query);
    }

    // If specified, allow a callback to modify the query.
    if (isset($this->options['callback'])) {
      call_user_func($this->options['callback'], $query, $ids, $conditions, $this->options);
    }
    return $query;
  }

  /**
   * @param unknown_type $ids
   * @param unknown_type $conditions
   */
  protected function buildQueryBase($ids, $conditions) {
    $query = db_select($this->baseTable, 'base');

    $query->addTag('versioncontrol_' . $this->entityType . '_load_multiple');

    // Add fields from the {entity} table.
    $entity_fields = drupal_schema_fields_sql($this->baseTable);

    $query->fields('base', $entity_fields);
    return $query;
  }

  /**
   * @param unknown_type $query
   * @param unknown_type $ids
   * @param unknown_type $conditions
   */
  protected function buildQueryConditions(&$query, $ids, $conditions) {
    // Attach conditions, starting with any IDs that were passed in.
    if ($ids) {
      $query->condition("base.{$this->idKey}", $ids, 'IN');
    }
    // If provided, attach generic conditions.
    if ($conditions) {
      foreach ($conditions as $field => $value) {
        $this->attachCondition($query, $field, $value);
      }
    }
  }

  /**
   * Attach a condition to a query being built, given a field and a value for
   * that field.
   *
   * @param SelectQuery $query
   * @param string $field
   * @param mixed $value
   */
  protected function attachCondition(&$query, $field, $value, $alias = 'base') {
    // If a condition value uses this special structure, we know the
    // requestor wants to do a complex condition with operator control.
    if (is_array($value) && isset($value['values']) && isset($value['operator'])) {
      $query->condition("$alias.$field", $value['values'], $value['operator']);
    }
    // Otherwise, we just pass the value straight in.
    else {
      $query->condition("$alias.$field", $value);
    }
  }

  /**
   * Add a new join to the in-progress query, if a join to that table does not
   * already exist.
   *
   * @param SelectQuery $query
   *   The query object to use.
   * @param string $table
   *   The real table name to be joined against.
   * @param string $requested_alias
   *   The requested alias to use.
   * @param string $join_logic
   *   The logic used to join the table to the query.
   * @return string $alias
   *   The alias of the joined table.
   */
  protected function addTable(&$query, $table, $requested_alias, $join_logic) {
    $alias = NULL;
    foreach ($query->getTables() as $table_data) {
      if ($table_data['table'] == $table) {
        $alias = $table_data['alias'];
      }
    }
    if (is_null($alias)) {
      $alias = $query->join($table, $requested_alias, $join_logic);
    }
    return $alias;
  }

  protected function addRepositoriesTable(&$query) {
    return $this->addTable($query, 'versioncontrol_repositories', 'vcr', "vcr.repo_id = base.repo_id");
  }

  protected function queryAlterGetBackendType(&$query) {
    if (!isset($this->backend)) {
      $this->addRepositoriesTable($query);
      $query->addField('vcr', 'vcs');
    }
  }

  /**
   * Optionally perform additional operations on the queried data before
   * building the entities in VersioncontrolEntityController::buildEntities().
   *
   * The default attaches a repository, if available, as every child class
   * except repositories themselves use it.
   *
   * @param array $queried_entities
   *   An associative array of stdClass objects, keyed on $this->idKey and
   *   containing the results of the query built by $this->buildQuery().
   */
  protected function extendData(&$queried_entities) {
    if ($this->options['repository'] instanceof VersioncontrolRepository) {
      foreach ($queried_entities as $entity) {
        // FIXME a lot of other code assumes that this always happens.
        $entity->repository = $this->options['repository'];
      }
    }
  }

  /**
   * Transform the queried data into the appropriate object types.
   *
   * Empty here because each entity type needs to specify their process.
   *
   * TODO We can use PDO to directly prepopulate objects, look into doing this: http://drupal.org/node/315092
   *
   * @param array $queried_entities
   */
  protected function buildEntities(&$queried_entities) {
    $built = array();
    foreach ($queried_entities as $entity) {
      $id = $entity->{$this->idKey};
      if (isset($this->backend)) {
        $built[$id] = $this->backend->buildObject($this->entityType, $entity);
      }
      else {
        $built[$id] = $this->backends[$entity->vcs]->buildObject($this->entityType, $entity);
      }
    }
    return $built;
  }

  /**
   * Get entities from the static cache.
   *
   * @param $ids
   *   If not empty, return entities that match these IDs.
   * @param $conditions
   *   If set, return entities that match all of these conditions.
   */
  protected function cacheGet($ids, $conditions) {
    $entities = array();
    // Load any available entities from the internal cache.
    if (!empty($this->entityCache)) {
      if ($ids) {
        $entities += array_intersect_key($this->entityCache, array_flip($ids));
      }
      // If loading entities only by conditions, fetch all available entities
      // from the cache. Entities which don't match are removed later.
      elseif ($conditions) {
        $entities = $this->entityCache;
      }
    }

    // Exclude any entities loaded from cache if they don't match $conditions.
    // This ensures the same behavior whether loading from memory or database.
    if ($conditions) {
      foreach ($entities as $entity) {
        // FIXME this probably needs to be more complex for our purposes
        $entity_values = (array) $entity;
        if (array_diff_assoc($conditions, $entity_values)) {
          unset($entities[$entity->{$this->idKey}]);
        }
      }
    }
    return $entities;
  }

  /**
   * Store entities in the static entity cache.
   */
  protected function cacheSet($entities, $rekey = FALSE) {
    if ($rekey) {
      $rekeyed = array();
      foreach ($entities as $entity) {
        $rekeyed[$entity->{$this->idKey}] = $entity;
      }
      $entities = $rekeyed;
    }
    $this->entityCache += $entities;
  }
}

class VersioncontrolRepositoryController extends VersioncontrolEntityController {
  protected $entityType = 'repo';
  protected $baseTable = 'versioncontrol_repositories';
  protected $idKey = 'repo_id';

  /**
   * Override the parent with an empty method, as repository objects can't
   * have a passed-in repository object.
   *
   * @param array $queried_entities
   */
  protected function extendData(&$queried_entities) {}
}

class VersioncontrolAccountController extends VersioncontrolEntityController {
  protected $entityType = 'account';
  protected $baseTable = 'versioncontrol_accounts';
  protected $idKey = 'repo_id'; // FIXME woah fugly. A lot needs to be reworked b/c it's got two primary keys
}

class VersioncontrolBranchController extends VersioncontrolEntityController {
  protected $entityType = 'branch';
  protected $baseTable = 'versioncontrol_labels';
  protected $idKey = 'label_id';

  protected function buildQueryConditions(&$query, $ids, $conditions) {
    parent::buildQueryConditions(&$query, $ids, $conditions);
    $this->attachCondition($query, 'type', VERSIONCONTROL_LABEL_BRANCH);
    return $query;
  }
}

class VersioncontrolTagController extends VersioncontrolEntityController {
  protected $entityType = 'tag';
  protected $baseTable = 'versioncontrol_labels';
  protected $idKey = 'label_id';

  protected function buildQueryConditions(&$query, $ids, $conditions) {
    parent::buildQueryConditions(&$query, $ids, $conditions);
    $this->attachCondition($query, 'type', VERSIONCONTROL_LABEL_TAG);
    return $query;
  }
}

class VersioncontrolOperationController extends VersioncontrolEntityController {
  protected $entityType = 'operation';
  protected $baseTable = 'versioncontrol_operations';
  protected $idKey = 'vc_op_id';

  protected function buildQueryConditions(&$query, $ids, $conditions) {
    // Attach conditions, starting with any IDs that were passed in.
    if ($ids) {
      $query->condition("base.{$this->idKey}", $ids, 'IN');
    }

    // The conditions passed in for Operations have special composition, and
    // require their own handling.
    foreach ($conditions as $type => $value) {
      // FIXME this is a huge, horrendous, overengineered list of things that
      // mostly are obsolete thanks to Views. keeping them all in here for now,
      // but each should be either filled out or removed as this evolves.
      switch ($type) {
        case 'vcs':
          $this->attachCondition($query, $type, $value, $this->addRepositoriesTable($query));
          break;

        case 'branches':
        case 'tags':
          $this->attachCondition($query, 'label_id', $value, $this->addLabelsTable($query));
          break;

        case 'types': // XX
        case 'revisions': // XX
        case 'labels':
        case 'paths':
        case 'message': // XX
        case 'item_revision_ids':
        case 'item_revisions':
        case 'vc_op_ids': // XX
        case 'uids': // XX
        case 'usernames':
        case 'user_relation':
        default:
          $this->attachCondition($query, $type, $value);
      }
    }
  }


  protected function addLabelsTable(&$query) {
    return $this->addTable($query, 'versioncontrol_operation_labels', 'vcol', "vcol.vc_op_id = base.vc_op_id");
  }

  /**
   * Attach a list of all associated labels to each operation in the set of
   * operations being loaded.
   *
   * @param array $queried_entities
   */
  protected function extendData(&$queried_entities) {
    parent::extendData($queried_entities);
    $query = db_select('versioncontrol_operations', 'vco')
      ->join('versioncontrol_operation_labels', 'vcol', 'vco.vc_op_id = vcol.vc_op_id')
      ->join('versioncontrol_labels', 'vcl', 'vcol.label_id = vcl.label_id')
      ->fields('vco', array('vc_op_id'))
      ->fields('vcol', array('action'))
      ->fields('vcl'); // add all the label fields
    // build the constraint list
    $operation_ids = array();
    foreach ($queried_entities as $entity_data) {
      $operation_ids[] = $entity_data->vc_op_id;
    }
    $ret = $query->condition('vco.vc_op_id', $operation_ids)
      ->execute();

    // Attach each label to the labels array on the respective operation data.
    // TODO consider actually building the branch/tag objects.
    foreach ($ret as $label) {
      $queried_entities[$label->vc_op_id]->labels[$label->name] = $label;
    }
  }
}

class VersioncontrolItemController extends VersioncontrolEntityController {
  protected $entityType = 'item';
  protected $baseTable = 'versioncontrol_items';
  protected $idKey = 'item_revision_id';
}

/**
 * Abstract parent class for all the various entity classes utilized by VC API.
 *
 * Basically just defines shared CRUD/loader-type behavior.
 */
abstract class VersioncontrolEntity implements ArrayAccess {
  protected $built = FALSE;

  /**
   * @var VersioncontrolRepository
   */
  protected $repository;

  /**
   * An instance of the Backend factory used to create this object, passed in
   * to the constructor. If this entity needs to spawn more entities, then it
   * should reuse this backend object to do so.
   *
   * @var VersioncontrolBackend
   */
  protected $backend;

  public function __construct($backend = NULL) {
    if ($backend instanceof VersioncontrolBackend) {
      $this->backend = $backend;
    }
    else if (variable_get('versioncontrol_single_backend_mode', FALSE)) {
      $backends = versioncontrol_get_backends();
      $this->backend = reset($backends);
    }
  }

  /**
   * Pseudo-constructor method; call this method with an associative array or
   * stdClass object containing properties to be assigned to this object.
   *
   * @param array $args
   */
  public function build($args = array()) {
    // If this object has already been built, bail out.
    if ($this->built == TRUE) {
      return FALSE;
    }

    foreach ($args as $prop => $value) {
      $this->$prop = $value;
    }
    if (!empty($this->data) && is_string($this->data)) {
      $this->data = unserialize($this->data);
    }
    $this->built = TRUE;
  }

  abstract public function save();

  abstract public function insert();

  abstract public function update();

  /**
   * Attach this entity's data to a query that will insert it into the database.
   *
   * This method is separated out to avoid code duplication between insert &
   * update queries, as well as making it possible to build up large queries
   * rather than issuing lots of small, single insert queries.
   *
   * @param Query $query
   */
  abstract public function buildSave(&$query);

  //ArrayAccess interface implementation FIXME soooooooo deprecated
  public function offsetExists($offset) {
    return isset($this->$offset);
  }
  public function offsetGet($offset) {
    return $this->$offset;
  }
  public function offsetSet($offset, $value) {
    $this->$offset = $value;
  }
  public function offsetUnset($offset) {
    unset($this->$offset);
  }

}
