<?php
// $Id$
/**
 * @file
 * Backend class
 */

/**
 * Backend base class
 *
 * @abstract
 */
abstract class VersioncontrolBackend {
  /**
   * The user-visible name of the VCS.
   *
   * @var string
   */
  public $name;

  /**
   * A short description of the backend, if possible not longer than
   * one or two sentences.
   *
   * @var string
   */
  public $description;

  /**
   * An array listing optional capabilities, in addition
   * to the required functionality like retrieval of detailed
   * commit information. Array values can be an arbitrary combination
   * of VERSIONCONTROL_CAPABILITY_* values. If no additional capabilities
   * are supported by the backend, this array will be empty.
   *
   * @var array
   */
  public $capabilities;

  /**
   * Classes which this backend will instantiate when acting as a factory.
   */
  public $classes = array();

  public function __construct() {
    // Add defaults to $this->classes
    // FIXME currently all these classes are abstract, so this won't work. Decide
    // if this should be removed, or if they should be made concrete classes
    $this->classes += array(
      'repo'      => 'VersioncontrolRepository',
      'account'   => 'VersioncontrolAccount',
      'operation' => 'VersioncontrolOperation',
      'item'      => 'VersioncontrolItem',
      'branch'    => 'VersioncontrolBranch',
      'tag'       => 'VersioncontrolTag',
    );
  }

  /**
   * Instantiate and build a VersioncontrolEntity object using provided data.
   *
   * This is the central factory method that should ultimately be used to
   * produce any VersioncontrolEntity-descended object for any backend. It does
   * two important things:
   *   - Provides a central point of control over what classes are used to
   *     instanciate what string 'type', as dictated by $this->classes.
   *   - Ensure the backend can handle the type requested, and that the class
   *     it wants to instantiate descends from VersioncontrolEntity.
   *
   * @param string $type
   *   A string indicating the type of entity to be created. Should match with a
   *   key in $this->classes.
   * @param mixed $data
   *   Either a stdClass object or an associative array of data to build the
   *   object with.
   * @return VersioncontrolEntity
   *   The instantiated and built object.
   */
  public function buildObject($type, $data) {
    // Ensure this backend knows how to handle the entity type requested
    if (empty($this->classes[$type])) {
      throw new Exception("Invalid entity type '$type' requested; not supported by current backend.");
    }

    // Ensure the class to create descends from VersioncontrolEntity.
    $class = $this->classes[$type];
    if (!is_subclass_of($class, 'VersioncontrolEntity')) {
      throw new Exception('Invalid Versioncontrol entity class specified for building; all entity classes descend from VersioncontrolEntity.', $class);
    }

    $obj = new $this->classes[$type]($this);
    $obj->build($data);
    return $obj;
  }

  /**
   * Augment a select query with options specific to this backend.
   *
   * This method is fired by entity controllers whenever the backend type is
   * known prior to the issuing of the query.
   *
   * @param SelectQuery $query
   *   The query object being built.
   * @param string $entity_type
   *   The type of entity being loaded.
   */
  public function augmentEntitySelectQuery($query, $entity_type) {}
}
