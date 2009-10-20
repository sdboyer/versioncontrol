<?php
// $Id$
/**
 * @file
 * Account class
 */

require_once 'VersioncontrolAccount.php';
require_once 'VersioncontrolRepository.php';

/**
 * Account class
 *
 * This class provides the way to manage users accounts.
 */
abstract class VersioncontrolAccount implements ArrayAccess {
  // Attributes
  /**
   * VCS's username
   *
   * @var    string
   */
  public $vcs_username;

  /**
   * Drupal user id
   *
   * @var    int
   */
  public $uid;

  /**
   * Repo user id
   *
   * @var    VersioncontrolRepository
   */
  public $repository;

  // Operations
  /**
   * Constructor
   */
  public function __construct($vcs_username, $uid, $repository = NULL) {
    $this->vcs_username = $vcs_username;
    $this->uid = $uid;
    $this->repository = $repository;
  }

  /**
   * Return the most accurate guess on what the VCS username for a Drupal user
   * might look like in the repository's account.
   *
   * @param $user
   *  The Drupal user who wants to register an account.
   */
  public function usernameSuggestion($user) {
    return strtr(drupal_strtolower($user->name),
      array(' ' => '', '@' => '', '.' => '', '-' => '', '_' => '', '.' => '')
    );
  }

  /**
   * Determine if the account repository allows a username to exist.
   *
   * @param $username
   *  The username to check. It is passed by reference so if the username is
   *  valid but needs minor adaptions (such as cutting away unneeded parts) then
   *  it the backend can modify it before returning the result.
   *
   * @return
   *   TRUE if the username is valid, FALSE if not.
   */
  public function isUsernameValid(&$username) {
    if (!preg_match('/^[a-zA-Z0-9]+$/', $username)) {
      return FALSE;
    }
    return TRUE;
  }

  /**
   * Update a VCS user account in the database, and call the necessary
   * module hooks. The account repository and uid must stay the same values as
   * the one given on account creation, whereas vcs_username and
   * @p $additional_data may change.
   *
   * @param $username
   *   The VCS specific username (a string). Here we are using an explicit
   *   parameter instead of taking the vcs_username data member to be able to
   *   verify is it changed, there would be lots of operations, so we do not
   *   want to update them if it's not necessary.
   * @param $additional_data
   *   An array of additional author information. Modules can fill this array
   *   by implementing hook_versioncontrol_account_submit().
   */
  public final function update($username, $additional_data = array()) {
    $repo_id = $this->repository->repo_id;
    $username_changed = ($username != $this->vcs_username);

    if ($username_changed) {
      $this->vcs_username = $username;
      db_query("UPDATE {versioncontrol_accounts}
                SET username = '%s'
                WHERE uid = %d AND repo_id = %d",
                $this->vcs_username, $this->uid, $repo_id
      );
    }

    // Provide an opportunity for the backend to add its own stuff.
    $this->_update($additional_data);

    if ($username_changed) {
      db_query("UPDATE {versioncontrol_operations}
                SET uid = 0
                WHERE uid = %d AND repo_id = %d",
                $this->uid, $repo_id);
      db_query("UPDATE {versioncontrol_operations}
                SET uid = %d
                WHERE committer = '%s' AND repo_id = %d",
                $this->uid, $this->vcs_username, $repo_id);
    }

    // Everything's done, let the world know about it!
    module_invoke_all('versioncontrol_account',
      'update', $this->uid, $this->vcs_username, $this->repository, $additional_data
    );

    watchdog('special',
      'Version Control API: updated @username account in repository @repository',
      array('@username' => $this->vcs_username, '@repository' => $this->repository->name),
      WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-accounts')
    );
  }

  /**
   * Let child backend account classes update information.
   */
  protected function _update($additional_data) {
  }

  /**
   * Insert a VCS user account into the database,
   * and call the necessary module hooks.
   *
   * @param $additional_data
   *   An array of additional author information. Modules can fill this array
   *   by implementing hook_versioncontrol_account_submit().
   */
  public final function insert($additional_data = array()) {
    db_query(
      "INSERT INTO {versioncontrol_accounts} (uid, repo_id, username)
       VALUES (%d, %d, '%s')", $this->uid, $this->repository->repo_id, $this->vcs_username
    );

    // Provide an opportunity for the backend to add its own stuff.
    $this->_insert($additional_data);

    // Update the operations table.
    // FIXME differenciate author and commiter
    db_query("UPDATE {versioncontrol_operations}
              SET uid = %d
              WHERE author = '%s' AND repo_id = %d",
              $this->uid, $this->vcs_username, $this->repository->repo_id);

    // Everything's done, let the world know about it!
    module_invoke_all('versioncontrol_account',
      'insert', $this->uid, $this->vcs_username, $this->repository, $additional_data
    );

    watchdog('special',
      'Version Control API: added @vcs_username account in repository @repository',
      array('@vcs_username' => $this->vcs_username, '@repository' => $this->repository->name),
      WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-accounts')
    );
  }

  /**
   * Let child backend account classes add information
   */
  protected function _insert($additional_data) {
  }

  /**
   * Delete a VCS user account from the database, set all commits with this
   * account as author to user 0 (anonymous), and call the necessary hooks.
   */
  public final function delete() {
    // Update the operations table.
    db_query('UPDATE {versioncontrol_operations}
              SET uid = 0
              WHERE uid = %d AND repo_id = %d',
              $this->uid, $this->repository->repo_id);

    // Announce deletion of the account before anything has happened.
    module_invoke_all('versioncontrol_account',
      'delete', $this->uid, $this->vcs_username, $this->repository, array()
    );

    // Provide an opportunity for the backend to delete its own stuff.
    $this->_delete();

    db_query('DELETE FROM {versioncontrol_accounts}
              WHERE uid = %d AND repo_id = %d',
              $this->uid, $this->repository->repo_id);

    watchdog('special',
      'Version Control API: deleted @username account in repository @repository',
      array('@username' => $this->vcs_username, '@repository' => $this->repository->name),
      WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-accounts')
    );
  }

  /**
   * Let child backend account classes delete information.
   */
  protected function _delete($additional_data) {
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
