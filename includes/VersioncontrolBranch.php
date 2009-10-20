<?php
// $Id$
/**
 * @file
 * Repo Branch class
 */

require_once 'VersioncontrolLabel.php';

/**
 * Represents a branch of code
 */
class VersioncontrolBranch extends VersioncontrolLabel {
  // Operations
  /**
   * Constructor
   */
  public function __construct($name, $action, $label_id = NULL, $repository = NULL) {
    parent::__construct(VERSIONCONTROL_LABEL_BRANCH, $name, $action, $label_id, $repository);
  }

}
