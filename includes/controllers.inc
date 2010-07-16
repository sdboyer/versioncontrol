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
  protected $cache = TRUE;
  protected $backends = array();

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

  public function load($ids = array(), $conditions = array()) {
    $entities = array();

    // Create a new variable which is either a prepared version of the $ids
    // array for later comparison with the entity cache, or FALSE if no $ids
    // were passed. The $ids array is reduced as items are loaded from cache,
    // and we need to know if it's empty for this reason to avoid querying the
    // database when all requested entities are loaded from cache.
    $passed_ids = !empty($ids) ? array_flip($ids) : FALSE;
    // Try to load entities from the static cache.
    if ($this->cache) {
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
      $queried_entities = $query
        ->execute()
        ->fetchAllAssoc($this->idKey);
    }

    if (!empty($queried_entities)) {
      $built_entities = $this->buildEntities($queried_entities);
      $entities += $built_entities;
    }

    if ($this->cache) {
      // Add entities to the cache.
      if (!empty($built_entities)) {
        $this->cacheSet($built_entities);
      }
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
  protected function buildQuery($ids, $conditions = array()) {
    $query = db_select($this->baseTable, 'base');

    $query->addTag($this->entityType . '_load_multiple');

    // Add fields from the {entity} table.
    $entity_fields = drupal_schema_fields_sql($this->baseTable);

    $query->fields('base', $entity_fields);

    if ($ids) {
      $query->condition("base.{$this->idKey}", $ids, 'IN');
    }
    if ($conditions) {
      foreach ($conditions as $field => $value) {
        // If a condition value uses this special structure, we know the
        // requestor wants to do a complex condition with operator control.
        if (is_array($value) && isset($value['values']) && isset($value['operator'])) {
          $query->condition('base.' . $field, $value['values'], $value['operator']);
        }
        // Otherwise, we just pass the value straight in.
        else {
          $query->condition('base.' . $field, $value);
        }
      }
    }
    if ($this->backend instanceof VersioncontrolBackend) {
      // Allow the current backend to augment the query as needed.
      $this->backend->augmentEntitySelectQuery($query, $this->entityType);
    }
    else {
      $this->queryAlterGetBackendType($query);
    }
    return $query;
  }

  protected function queryAlterGetBackendType($query) {
    if (!isset($this->backend)) {
      // Add a join to the repo table so we know which backend to use.
      $query->join('versioncontrol_repositories', 'vcr', "vcr.repo_id = base.repo_id");
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
    foreach ($queried_entities as $id => $entity) {
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
  protected function cacheGet($ids, $conditions = array()) {
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
  protected function cacheSet($entities) {
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
  protected $entityType = 'label';
  protected $baseTable = 'versioncontrol_labels';
  protected $idKey = 'label_id';

  protected function buildQuery($ids, $conditions = array()) {
    $query = parent::buildQuery($ids, $conditions);
    $query->condition('base', 'type', VERSIONCONTROL_LABEL_BRANCH);
    return $query;
  }
}

class VersioncontrolTagController extends VersioncontrolEntityController {
  protected $entityType = 'label';
  protected $baseTable = 'versioncontrol_labels';
  protected $idKey = 'label_id';

  protected function buildQuery($ids, $conditions = array()) {
    $query = parent::buildQuery($ids, $conditions);
    $query->condition('base', 'type', VERSIONCONTROL_LABEL_TAG);
    return $query;
  }
}

class VersioncontrolOperationController extends VersioncontrolEntityController {
  protected $entityType = 'operation';
  protected $baseTable = 'versioncontrol_operations';
  protected $idKey = 'vc_op_id';
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
    if (!$backend instanceof VersioncontrolBackend) {
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
