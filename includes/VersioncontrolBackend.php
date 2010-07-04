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
abstract class VersioncontrolBackend implements ArrayAccess {
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
   * classes which this backend overwrite
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

  public function buildObject($type, $data) {
    $class = $this->classes[$type];
    if (!is_subclass_of($class, VersioncontrolEntity)) {
      throw new Exception('Invalid Versioncontrol entity class specified; all entity classes should have VersioncontrolEntity as a parent', $class);
    }
    $obj = new $this->classes[$type]($this);
    $obj->build($data);
    return $obj;
  }

//  public function buildQueryRepository($query) {}
//
//  public function buildQueryLabel($query) {}
//
//  public function buildQueryOperation($query) {}
//
//  public function buildQueryAccount($query) {}
//
//  public function buildQueryItem($query) {}

  //ArrayAccess interface implementation
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
