<?php
// $Id$
/**
 * @file
 * Repo Branch class
 */

/**
 * Represents a repository branch.
 */
class VersioncontrolBranch extends VersioncontrolEntity {
  /**
   * The tag identifier (a simple integer), used for unique identification of
   * this tag in the database.
   *
   * @var int
   */
  public $label_id;

  /**
   * The tag name.
   *
   * @var string
   */
  public $name;

  /**
   * Indicates this is a branch; for db interaction only.
   *
   * @var int
   */
  public $type = VERSIONCONTROL_LABEL_BRANCH;

  /**
   * @name VCS actions
   * for a single item (file or directory) in a commit, or for branches and tags.
   * either VERSIONCONTROL_ACTION_{ADDED,MODIFIED,MOVED,COPIED,MERGED,DELETED,
   * REPLACED,OTHER}
   *
   * @var array
   */
  public $action;

  /**
   * The database id of the repository with which this branch is associated.
   * @var int
   */
  public $repo_id;

  /**
   * Insert a tag entry into the {versioncontrol_labels} table, or retrieve the
   * same one that's already there.
   *
   * The object is enhanced with the newly added property 'label_id' specifying
   * the database identifier for that label. There may be labels with a similar
   * 'name' but different 'type' properties, those are considered to be
   * different and will both go into the database side by side.
   *
   * @deprecated FIXME remove this approach, it leads to inefficient single-loading.
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
  public function insert() {
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
  public function save() {}
  public function update() {}
  public function buildSave(&$query) {}
}
