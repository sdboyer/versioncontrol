<?php
// $Id$
/**
 * @file
 * Repo Tag class
 */

require_once 'VersioncontrolLabel.php';

/**
 * Represents a tag of code(not changing state)
 */
class VersioncontrolTag extends VersioncontrolLabel {

  // Operations
  /**
   * Constructor
   */
  public function __construct($name, $action, $label_id = NULL, $repository = NULL) {
    parent::__construct(VERSIONCONTROL_LABEL_TAG, $name, $action, $label_id, $repository);
  }

}
