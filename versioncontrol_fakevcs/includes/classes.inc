<?php

require_once drupal_get_path('module', 'versioncontrol') . '/includes/VersioncontrolBackend.php';

class VersioncontrolFakeBackend extends VersioncontrolBackend {

  public function __construct() {
    $this->name = 'FakeVCS';
    $this->description = t('FakeVCS is a version control system that is specifically capable in doing everything that any other version control system might ever do.');
    // Our example VCS supports all possible capabilities.
    $this->capabilities = array(
        // Able to cancel commits if the committer lacks permissions
        // to commit to specific paths and/or branches.
        VERSIONCONTROL_CAPABILITY_COMMIT_RESTRICTIONS,

        // Able to cancel branch or tag assignments if the committer lacks
        // permissions to create/update/delete those.
        VERSIONCONTROL_CAPABILITY_BRANCH_TAG_RESTRICTIONS,

        // Able to retrieve a file or its revision number based on a global
        // revision identifier.
        VERSIONCONTROL_CAPABILITY_ATOMIC_COMMITS,

        // The version control system assigns revisions not only to files
        // but also to directories.
        VERSIONCONTROL_CAPABILITY_DIRECTORY_REVISIONS,
    );
    $this->classes = array(
      'repo'      => 'VersioncontrolFakeRepository',
      'account'   => 'VersioncontrolFakeAccount',
      'operation' => 'VersioncontrolFakeOperation',
      'item'      => 'VersioncontrolFakeItem',
    );
  }

}

class VersioncontrolFakeRepository extends VersioncontrolRepository implements VersioncontrolRepositoryImportExport, VersioncontrolRepositoryGetItem {

  public function formatRevisionIdentifier($revision, $format = 'full') {
    switch ($format) {
    case 'full':
    case 'short':
    default:
      // Suppose we're a distributed VCS backend and have an SHA-1 hash:
      // $revision == '30581e4ec3347d1294ec05a91eec1a8588e5993c'
      // Let's return only the first 12 characters of the revision identifier,
      // like Mercurial (including hgweb) does by default.
      return substr($revision, 0, 12);

      // SVN also wants to format revisions in a slightly custom way:
      return 'r'. $revision;
    }
  }

  public function _getItem($path, $constraints = array()) {
    // Slightly adapted version of the SVN backend's implementation.

    // Wherever you get your item info from.
    $revision = empty($constraints['revision']) ? 'HEAD' : $constraints['revision'];
    $info = $this->getItemInfo($path, $revision);

    $type = ($info['rev'] == 'dir')
      ? VERSIONCONTROL_ITEM_DIRECTORY
      : VERSIONCONTROL_ITEM_FILE;
    $item = new $this->backend->classes['item']($type, $path, $info['rev'], NULL, $this, NULL, $info['item_revision_id']);
    return array('item' => $item, 'selected_label' => NULL);
  }

  public function importAccounts($data) {
    $accounts = $this->parseAccountData($data);

    foreach ($accounts as $account) {
      $additional_data = array(
        'fakevcs_specific' => array('password' => $password),
      );
      $username = $account->author;
      $uid = $this->getAccountUidForUsername($username, TRUE);

      if (isset($uid)) {
        $account->uid = $uid;
        $account->update($additional_data);
        $names[] = t('updated !username', array('!username' => $username));
      }
      else {
        $uid = db_result(db_query("SELECT uid FROM {users} WHERE name = '%s'", $username));
        if ($uid) {
          $account->uid = $uid;
          $account->insert($additional_data);
          $names[] = t('added !username', array('!username' => $username));
        }
        else {
          $names[] = t('didn\'t add !username (no matching Drupal username exists)',
            array('!username' => $username));
        }
      }
    }
  }

  public function exportAccounts($accounts) {
    if (empty($accounts)) {
      return '# '. t('no user accounts available to export');
    }
    $accounts_flat = array();
    $uid_constraints = array();
    $params = array($this->repo_id);

    foreach ($accounts as $uid => $accounts_per_repository) {
      foreach ($accountss_per_repository as $repo_id => $account) {
        $accounts_flat[$uid] = array('uid' => $uid, 'author' => $account->author);
        $uid_constraints[] = 'uid = %d';
        $params[] = $uid;
      }
    }

    $result = db_query('SELECT uid, password FROM {versioncontrol_fakevcs_accounts}
      WHERE repo_id = %d
      AND ('. implode(' OR ', $uid_constraints) .')',
      $params);
    while ($account = db_fetch_object($result)) {
      $accounts_flat[$account->uid]['password'] = $account->password;
    }

    $data = '';
    foreach ($accounts_flat as $uid => $account) {
      $data .= '# '. url('user/'. $uid, array('absolute' => TRUE)) ."\n";
      $data .= $account['author'] .':'. $account['password'] ."\n";
    }
    return $data;
  }

}

class VersioncontrolFakeAccount extends VersioncontrolAccount {

  /**
   * Overwrite
   */
  public function usernameSuggestion($user) {
    // For distributed version control systems, the user's email address
    // might be a more appropriate username than the actual nick - it
    // guarantees unique identification (= uid association for
    // operations), even though it might need to be replaced by or
    // amended with the full name on page display.
    return $user->mail;
  }

  /**
   * Overwrite
   */
  function isUsernameValid(&$username) {
    // Continuing the email-style username example from above.
    return valid_email_address($username);
  }

  /**
   * Overwrite
   */
  protected function _insert($additional_data) {
    $fakevcs_specific = $additional_data['fakevcs_specific'];
    if (!isset($fakevcs_specific) || !isset($fakevcs_specific['password'])) {
      drupal_set_message(t('Error: no FakeVCS password given on account creation!'), 'error');
      return;
    }
    db_query("INSERT INTO {versioncontrol_fakevcs_accounts}
      (uid, repo_id, password)
      VALUES (%d, %d, '%s')",
      $this->uid, $this->repository->repo_id, $fakevcs_specific['password']);
  }

  /**
   * Overwrite
   */
  protected function _update($additional_data) {
    $fakevcs_specific = $additional_data['fakevcs_specific'];
    if (!isset($fakevcs_specific) || !isset($fakevcs_specific['password'])) {
      return; // the user didn't update the password in the process.
    }
    db_query("UPDATE {versioncontrol_fakevcs_accounts}
      SET password = '%s'
      WHERE uid = %d AND repo_id = %d",
    $fakevcs_specific['password'], $this->uid, $this->repository->repo_id);

    if (!versioncontrol_admin_access()) {
      // Admins get "The account has been updated successfully" anyways.
      drupal_set_message(t('The FakeVCS password has been updated successfully.'));
    }
  }

  /**
   * Overwrite
   */
  protected function _delete($additional_data) {
   db_query('DELETE FROM {versioncontrol_fakevcs_accounts}
     WHERE uid = %d AND repo_id = %d',
     $this->uid, $this->repository->repo_id);
  }

}

class VersioncontrolFakeOperation extends VersioncontrolOperation {

  /**
   * Implementation of abstract method.
   */
  public function getSelectedLabel($target_item) {
    // How CVS and many other version control systems will probably do it,
    // as they have exactly one label assigned to each operation.
    return $operation->labels[0];

    // If an operation - or more specifically, a commit - applies to multiple
    // branches and/or tags (as can happen in SVN, for example), then the correct
    // label for each item has to be determined.
  }

}

class VersioncontrolFakeItem
extends VersioncontrolItem
implements VersioncontrolItemParallelItems, VersioncontrolItemDirectoryContents, VersioncontrolItemExportFile, VersioncontrolItemExportDirectory, VersioncontrolItemGetFileAnnotation {

  /**
   * Implementation of abstract method.
   */
  public function getSelectedLabelFromItem(&$other_item, $other_item_tags = array()) {
    // First up, optimizations - maybe we can do without the generic
    // "label transfer" code from further down and use assumptions
    // instead.

    // Let's assume for FakeVCS repositories that if an item wears a
    // label, then an item at another path but with the same (file-level)
    // revision can also wear that same label. That is the case with some
    // version control systems (e.g. Git, Mercurial, Bazaar) but might
    // not be the case with others (CVS for its lack of global revision
    // identifiers, SVN for its use of directory structure as tag/branch
    // identifiers).
    if ($item->revision == $other_item->revision) {
      return $other_item->getSelectedLabel();
    }

    // If the $other_item is a successor item on a branch, some version
    // control systems (like CVS) can guarantee that the $target_item,
    // the predecessor, is always on the same branch. So no need for
    // database queries, yay.
    if (in_array('successor_item', $other_item_tags)) {
      $label = $other_item->getSelectedLabel();

      if ($label instanceof VersioncontrolBranch) {
        return $label;
      }
    }

    // Otherwise we might not be able to derive the $target_item's label,
    // in which case we need to fall back to the database or a VCS
    // invocation.  For example, something like this.
    if ($target_item->fetchItemRevisionId()) {
      $constraints = array(
        'item_revision_id' => array($target_item->item_revision_id),
      );
      $commit_operations = VersioncontrolOperationCache::getInstance()->getCommits($constraints);

      if (!empty($commit_operations)) { // yo, found the associated commit!
        // Code taken from the CVS backend, which only assigns a single
        // branch to any given commit operation, so it can just take the
        // first one.
        $commit_operation = reset($commit_operations); // first (only) element
        return $commit_operation->labels[0];
      }
    }

    // (You can even do more attempts if you want to, like trying to ask
    // the VCS itself. Or maybe not, depends on your motivation and the
    // VCS's capabilities.)

    // No label could be retrieved by looking at the other item, sorry.
    return NULL;
  }


  /**
   * Method for VersioncontrolItemParallelItems interface.
   * TODO: convert to OOP
   */
  function _getParallelItems($label_type_filter = NULL) {
    // a fake repo for our fake item in this example
    $_repo = array(
      'name' => 'A fake repo',
      'vcs'  => 'fakevcs',
      'root' => '/path/to/the/repo',
      'authorization_method' => 'versioncontrol_admin',
    );
    $repo = new VersioncontrolFakeRepository(1, $_repo, FALSE);

    // How CVS would probably do it, if for example
    // $item['path'] == '/contributions/modules/versioncontrol/versioncontrol.module':
    return array(
      array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/contributions/modules/versioncontrol/versioncontrol.module',
          '1.23',
          NULL,
          $repo
        ),
        'selected_label' => new VersioncontrolBranch('HEAD', NULL),
      ),
      array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/contributions/modules/versioncontrol/versioncontrol.module',
          '1.23.2.42',
          NULL,
          $repo
        ),
        'selected_label' => new VersioncontrolBranch('DRUPAL-5--1', NULL),
      ),
      array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/contributions/modules/versioncontrol/versioncontrol.module',
          '1.23.2.42',
          NULL,
          $repo
        ),
        'selected_label' => new VersioncontrolBranch('DRUPAL-5--1-2', NULL),
      ),
    );

    // How SVN could also do it, if for example
    // $item['path'] == '/trunk/contributions/modules/versioncontrol':
    return array(
      array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_DIRECTORY,
          '/trunk/contributions/modules/versioncontrol',
          '23',
          NULL,
          $repo
        ),
        'selected_label' => new VersioncontrolBranch('trunk', NULL),
      ),
      array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_DIRECTORY,
          '/branches/5.x-1.x/contributions/modules/versioncontrol',
          '42',
          NULL,
          $repo
        ),
        'selected_label' => new VersioncontrolBranch('5.x-1.x', NULL),
      ),
    );

  }

  /**
   * Method for VersioncontrolItemDirectoryContents interface.
   */
  public function _getDirectoryContents($recursive = FALSE) {
    // a fake repo for our fake item in this example
    $_repo = array(
      'name' => 'A fake repo',
      'vcs'  => 'fakevcs',
      'root' => '/path/to/the/repo',
      'authorization_method' => 'versioncontrol_admin',
    );
    $repo = new VersioncontrolFakeRepository(1, $_repo, FALSE);

    // Assuming this object looks like this:
    $directory_item = new VersioncontrolFakeItem(
      VERSIONCONTROL_ITEM_DIRECTORY, '/trunk/contributions/modules/versioncontrol', '777', NULL, $repo
    );
    // $repository is some SVN repository and $recursive == FALSE. SVN without
    // branch/tag emulation can return NULL for all the selected labels.

    return array(
      '/trunk/contributions/modules/versioncontrol' => array(
        'item' => new VersioncontrolFakeItem( // == rtrim($path, '/')
          VERSIONCONTROL_ITEM_DIRECTORY,
          '/trunk/contributions/modules/versioncontrol',
          '502',
          NULL,
          $repo
        ),
        'selected_label' => NULL,
      ),
      '/trunk/contributions/modules/versioncontrol/versioncontrol.module' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/trunk/contributions/modules/versioncontrol/versioncontrol.module',
          '502',
          NULL,
          $repo
        ),
        'selected_label' => NULL,
      ),
      '/trunk/contributions/modules/versioncontrol/versioncontrol.info' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/trunk/contributions/modules/versioncontrol/versioncontrol.info',
          '404',
          NULL,
          $repo
        ),
        'selected_label' => NULL,
      ),
      '/trunk/contributions/modules/versioncontrol/versioncontrol.install' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/trunk/contributions/modules/versioncontrol/versioncontrol.install',
          '404',
          NULL,
          $repo
        ),
        'selected_label' => NULL,
      ),
      '/trunk/contributions/modules/versioncontrol/README.txt' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/trunk/contributions/modules/versioncontrol/README.txt',
          '404',
          NULL,
          $repo
        ),
        'selected_label' => NULL,
      ),
      '/trunk/contributions/modules/versioncontrol/versioncontrol_fakevcs' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_DIRECTORY,
          '/trunk/contributions/modules/versioncontrol/versioncontrol_fakevcs.txt',
          '497',
          NULL,
          $repo
        ),
        'selected_label' => NULL,
      ),
    );

    // Or the same thing in CVS - note that 'revision' is an empty string
    // for directories, as CVS doesn't support versioned directories.
    $directory_item = new VersioncontrolFakeItem(
      VERSIONCONTROL_ITEM_DIRECTORY, '/contributions/modules/versioncontrol', '', NULL, $repo
    );
    // $repository is some CVS repository and $recursive == TRUE.
    // For the purpose of example, we use the same label as the directory item.
    // (Real-life usage might or might not require more correctness checks.)
    $selected_label = $this->getSelectedLabel();

    return array(
      '/contributions/modules/versioncontrol' => array(
        'item' => new VersioncontrolFakeItem( // == rtrim($path, '/')
          VERSIONCONTROL_ITEM_DIRECTORY,
          '/contributions/modules/versioncontrol',
          '',
          NULL,
          $repo
        ),
        'selected_label' => $selected_label,
      ),
      '/contributions/modules/versioncontrol/versioncontrol.module' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/contributions/modules/versioncontrol/versioncontrol.module',
          '1.19',
          NULL,
          $repo
        ),
        'selected_label' => $selected_label,
      ),
      '/contributions/modules/versioncontrol/versioncontrol.info' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/contributions/modules/versioncontrol/versioncontrol.info',
          '1.1',
          NULL,
          $repo
        ),
        'selected_label' => $selected_label,
      ),
      '/contributions/modules/versioncontrol/versioncontrol.install' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/contributions/modules/versioncontrol/versioncontrol.install',
          '1.5',
          NULL,
          $repo
        ),
        'selected_label' => $selected_label,
      ),
      '/contributions/modules/versioncontrol/README.txt' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/contributions/modules/versioncontrol/README.txt',
          '1.1',
          NULL,
          $repo
        ),
        'selected_label' => $selected_label,
      ),
      '/contributions/modules/versioncontrol/fakevcs_backend' => array(
        'item' => new VersioncontrolFakeItem( // == rtrim($path, '/')
          VERSIONCONTROL_ITEM_DIRECTORY,
          '/contributions/modules/versioncontrol/fakevcs_backend',
          '',
          NULL,
          $repo
        ),
        'selected_label' => $selected_label,
      ),
      '/contributions/modules/versioncontrol/fakevcs_backend/fakevcs_backend.module' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE_DELETED, // only for non-atomic-commit VCSs (= CVS),
          '/contributions/modules/versioncontrol/fakevcs_backend/fakevcs_backend.module',
          '1.11',
          NULL,
          $repo
        ),
        'selected_label' => $selected_label,
      ),
      '/contributions/modules/versioncontrol/versioncontrol_fakevcs' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_DIRECTORY,
          '/contributions/modules/versioncontrol/versioncontrol_fakevcs',
          '',
          NULL,
          $repo
        ),
        'selected_label' => $selected_label,
      ),
      '/contributions/modules/versioncontrol/versioncontrol_fakevcs/versioncontrol_fakevcs.module' => array(
        'item' => new VersioncontrolFakeItem(
          VERSIONCONTROL_ITEM_FILE,
          '/contributions/modules/versioncontrol/versioncontrol_fakevcs/versioncontrol_fakevcs.module',
          '1.2',
          NULL,
          $repo
        ),
        'selected_label' => $selected_label,
      ),
    );

  }

  /**
   * Method for VersioncontrolItemExportFile interface.
   */
  public function _exportFile($destination) {
    exec('fakevcs cat '. $this->repository->root . $this->path .' > '. $destination,
      $output, $return_code);

    if ($return_code != 0) {
      return FALSE;
    }
    return TRUE;
  }

  /**
   * Method for VersioncontrolItemExportDirectory interface.
   */
  public function _exportDirectory($destination_dirpath) {
    exec('fakevcs export '. $this->repository->root . $this->path
      .' '. $destination_dirpath, $output, $return_code);

    if ($return_code != 0) {
      return FALSE;
    }
    return TRUE;
  }

  /**
   * Method for VersioncontrolItemGetFileAnnotation interface.
   */
  function _getFileAnnotation() {
    // In case the file is marked as text file:
    return array( // using Drupal COPYRIGHT.txt file as example
      1 => array(
        'username' => 'dries',
        'line'     => '// $Id$',
      ),
      2 => array(
        'username' => 'dries',
        'line'     => '',
      ),
      3 => array(
        'username' => 'dries',
        'line'     => 'All Drupal code is Copyright 2001 - 2008 by the original authors.',
      ),
    );
  }

}
