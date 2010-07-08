<?php
// $Id$
/**
 * @file
 * We need to declare this file to avoid a not defined class, because autoload
 * is executed __after__ bootstrap, where drupal_load get all php
 */

class VersioncontrolTestBackend extends VersioncontrolBackend {
  public function __construct() {
    $this->name = 'TestVCS';
    $this->description = t('TestVCS backend for Version Control API.');
    $this->capabilities = array(
      // Use the commit hash for to identify the commit instead of an individual
      // revision for each file.
      VERSIONCONTROL_CAPABILITY_ATOMIC_COMMITS
    );
    $this->classes = array(
      'repo' => 'VersioncontrolTestRepository',
      'account' => 'VersioncontrolTestAccount',
      'operation' => 'VersioncontrolTestOperation',
      'item' => 'VersioncontrolTestItem',
    );
  }
}

class VersioncontrolTestRepository extends VersioncontrolRepository {
}

class VersioncontrolTestAccount extends VersioncontrolAccount {
}

class VersioncontrolTestOperation extends VersioncontrolOperation {

  /**
   * Implementation of abstract method.
   */
  public function getSelectedLabel($target_item) {
    return $operation->labels[0];
  }

}

class VersioncontrolTestItem extends VersioncontrolItem {

  /**
   * Implementation of abstract method.
   * Now we do not test fecth from repo, so this method is empty. So,
   * when it's needed see fakevcs backend implementation for reference.
   */
  public function getSelectedLabelFromItem(&$other_item, $other_item_tags = array()) {
    $_repo = array(
      'name' => 'A test repo',
      'vcs'  => 'fakevcs',
      'root' => '/path/to/the/repo',
      'authorization_method' => 'versioncontrol_admin',
    );
    $repo = new VersioncontrolTestRepository(1, $_repo, FALSE);
    return new VersioncontrolBranch('HEAD', NULL, '1', $repo);
  }

}
