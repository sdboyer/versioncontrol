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
  /**
   * An array of default options appended to options passed in by the caller.
   * @var array
   */
  protected $defaultOptions = array(
    'determine backend' => TRUE,
    'may cache' => TRUE,
    'callback' => NULL,
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
    $options += $this->defaultOptions;

    // Create a new variable which is either a prepared version of the $ids
    // array for later comparison with the entity cache, or FALSE if no $ids
    // were passed. The $ids array is reduced as items are loaded from cache,
    // and we need to know if it's empty for this reason to avoid querying the
    // database when all requested entities are loaded from cache.
    $passed_ids = !empty($ids) ? array_flip($ids) : FALSE;
    // Try to load entities from the static cache.
    if ($options['may cache']) {
      $entities += $this->cacheGet($ids, $conditions, $options);
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
      $query = $this->buildQuery($ids, $conditions, $options);
      $queried_entities = $query
        ->execute()
        ->fetchAllAssoc($this->idKey);
    }

    if (!empty($queried_entities)) {
      $built_entities = $this->buildEntities($queried_entities);
      $entities += $built_entities;
    }

    if ($options['may cache'] && !empty($built_entities)) {
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
  protected function buildQuery($ids, $conditions, $options) {
    $query = $this->buildQueryBase($ids, $conditions, $options);
    $this->buildQueryConditions($query, $ids, $conditions, $options);

    // Allow augmentation of the query by the current backend, if possible.
    if ($this->backend instanceof VersioncontrolBackend) {
      // Allow the current backend to augment the query as needed.
      $this->backend->augmentEntitySelectQuery($query, $this->entityType);
    }
    // Or determine the backend for these entities in the query, unless query
    // options tell us not to.
    else if ($options['determine backend'] === TRUE) {
      $this->queryAlterGetBackendType($query);
    }

    // If specified, allow a callback to modify the query.
    if (isset($options['callback'])) {
      call_user_func($options['callback'], $query, $ids, $conditions, $options);
    }
    return $query;
  }

  protected function buildQueryBase($ids, $conditions, $options) {
    $query = db_select($this->baseTable, 'base');

    $query->addTag('versioncontrol_' . $this->entityType . '_load_multiple');

    // Add fields from the {entity} table.
    $entity_fields = drupal_schema_fields_sql($this->baseTable);

    $query->fields('base', $entity_fields);
    return $query;
  }

  protected function buildQueryConditions(&$query, $ids, $conditions, $options) {
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

  protected function addRepositoriesTable(&$query) {
    $alias = NULL;
    foreach ($query->getTables() as $table_data) {
      if ($table_data['table'] == 'versioncontrol_repositories') {
        $alias = $table_data['alias'];
      }
    }
    if (is_null($alias)) {
      $query->join('versioncontrol_repositories', 'vcr', "vcr.repo_id = base.repo_id");
    }
    return $alias;
  }

  protected function queryAlterGetBackendType(&$query) {
    if (!isset($this->backend)) {
      $this->addRepositoriesTable($query);
      $query->addField('vcr', 'vcs');
    }
  }

  /**
   * Transform the queried data into the appropriate object types.
   *
   * Empty here because each entity type needs to specify their process.
   *
   * @param array $queried_entities
   */
  protected function buildEntities(&$queried_entities) {
    $built = array();
    foreach ($queried_entities as $entity) {
      // Ensure we cache using the expected id key
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
  protected function cacheGet($ids, $conditions, $options) {
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

  protected function buildQueryConditions(&$query, $ids, $conditions, $options) {
    parent::buildQueryConditions(&$query, $ids, $conditions, $options);
    $this->attachCondition($query, 'type', VERSIONCONTROL_LABEL_BRANCH);
    return $query;
  }
}

class VersioncontrolTagController extends VersioncontrolEntityController {
  protected $entityType = 'tag';
  protected $baseTable = 'versioncontrol_labels';
  protected $idKey = 'label_id';

  protected function buildQueryConditions(&$query, $ids, $conditions, $options) {
    parent::buildQueryConditions(&$query, $ids, $conditions, $options);
    $this->attachCondition($query, 'type', VERSIONCONTROL_LABEL_TAG);
    return $query;
  }
}

class VersioncontrolOperationController extends VersioncontrolEntityController {
  protected $entityType = 'operation';
  protected $baseTable = 'versioncontrol_operations';
  protected $idKey = 'vc_op_id';

  protected function buildQueryConditions(&$query, $ids, $conditions, $options) {
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
          $alias = $this->addRepositoriesTable($query);
          $this->attachCondition($query, $type, $value, $alias);
          break;

        case 'repo_id':
        case 'types':
        case 'branches':
        case 'tags':
        case 'revisions':
        case 'labels':
        case 'paths':
        case 'message':
        case 'item_revision_ids':
        case 'item_revisions':
        case 'vc_op_ids':
        case 'date_lower':
        case 'date_upper':
        case 'uids':
        case 'usernames':
        case 'user_relation':
        default:
          $this->attachCondition($query, $type, $value);
      }
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
abstract class VersioncontrolEntity {
  protected $built = FALSE;

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
   * Pseudo-constructor method; call this method with an associative array of
   * properties to be assigned to this object.
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
    if (is_string($this->data)) {
      $this->data = unserialize($this->data);
    }
    $this->built = TRUE;
  }
}
