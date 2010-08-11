<?php
// $Id$
/**
 * @file
 * Repo Label class
 */

/**
 * The parent of branches and tags classes
 */
abstract class VersioncontrolLabel implements ArrayAccess {
  // Attributes
  /**
   * The label identifier (a simple integer), used for unique
   * identification of branches and tags in the database.
   *
   * @var    int
   */
  public $label_id;

  /**
   * The branch or tag name.
   *
   * @var    string
   */
  public $name;

  /**
   * The id of the repository with which this label is associated.
   *
   * @var int
   */
  public $repository;

  /**
   * Whether this label is a branch (indicated by the
   * VERSIONCONTROL_LABEL_BRANCH constant) or a tag
   * (VERSIONCONTROL_LABEL_TAG).
   *
   * @var    int
   */
  public $type;

  /**
   * @name VCS actions
   * for a single item (file or directory) in a commit, or for branches and tags.
   * either VERSIONCONTROL_ACTION_{ADDED,MODIFIED,MOVED,COPIED,MERGED,DELETED,
   * REPLACED,OTHER}
   *
   * @var    array
   */
  public $action;

  // Associations
  // Operations

  /**
   * Insert a label entry into the {versioncontrol_labels} table,
   * or retrieve the same one that's already there.
   *
   * The object is enhanced with the newly added property 'label_id'
   * specifying the database identifier for that label. There may be labels
   * with a similar 'name' but different 'type' properties, those are considered
   * to be different and will both go into the database side by side.
   */
  public function ensure() {
    if (!empty($this->label_id)) { // already in the database
      return;
    }
    $result = db_result(db_query("SELECT label_id FROM {versioncontrol_labels} WHERE repo_id = %d AND name = '%s' AND type = %d",
      $this->repository->repo_id, $this->name, $this->type));
    if ($result) {
      $this->label_id = $result;
    }
    else {
      // The item doesn't yet exist in the database, so create it.
      $this->insert();
    }
  }

  /**
   * Insert label to db
   */
  protected function insert() {
    $this->repo_id = $this->repository->repo_id; // for drupal_write_record() only

    if (isset($this->label_id)) {
      // The label already exists in the database, update the record.
      drupal_write_record('versioncontrol_labels', $this, 'label_id');
    }
    else {
      // The label does not yet exist, create it.
      // drupal_write_record() also assigns the new id to $this->label_id.
      drupal_write_record('versioncontrol_labels', $this);
    }
    unset($this->repo_id);
  }

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
