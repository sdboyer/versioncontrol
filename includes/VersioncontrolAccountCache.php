<?php
// $Id$

/**
 * Acocunt loader, sigleton class.
 */
final class VersioncontrolAccountCache {
  private static $instance;
  private $accountCache = array('uid' => array(), 'repo_id' => array());

  /**
   * Internal state variable indicating whether or not all repositories have
   * been fetched (via VersioncontrolAccountCache::getInstance()->getAllAccounts()).
   * @var bool
   */
  private $allFetched = FALSE;

  private function __construct() {
    // TODO really oughtta make this better.
    $result = db_query('SELECT uid, repo_id, username FROM {versioncontrol_accounts}');
    // Cache a skeletal, low-mem overhead list of all the accounts we have.
    while ($item = db_fetch_object($result)) {
      $this->accountCache['uid'][$item->uid][$item->repo_id] = &$item;
      //$this->accountCache['repo_id'][$item->repo_id][$item->uid] = &$item;
    }
  }

  /**
   * Return the singleton's instance of the VersioncontrolAccountCache.
   *
   * @return VersioncontrolAccountCache
   */
  public static function getInstance() {
    if (!self::$instance instanceof VersioncontrolAccountCache) {
      self::$instance = new VersioncontrolAccountCache();
    }
    return self::$instance;
  }

  /**
   * Retrieve a set of Drupal uid / VCS username mappings
   * that match the given constraints.
   *
   * @static
   * @param $constraints
   *   An optional array of constraints. Possible array elements are:
   *
   *   - 'uids': An array of Drupal user ids. If given, only accounts that
   *        correspond to these Drupal users will be returned.
   *   - 'repo_ids': An array of repository ids. If given, only accounts
   *        in the corresponding repositories will be returned.
   *   - 'usernames': An array of system specific VCS usernames,
   *        like array('dww', 'jpetso'). If given, only accounts
   *        with these VCS usernames will be returned.
   *   - 'usernames_by_repository': A structured array that looks like
   *        array($repo_id => array('dww', 'jpetso'), ...).
   *        You might want this if you combine multiple username and repository
   *        constraints, otherwise you can well do without.
   *
   * @param $include_unauthorized
   *   If FALSE (which is the default), this function does not return accounts
   *   that are pending, queued, disabled, blocked, or otherwise non-approved.
   *   If TRUE, all accounts are returned, regardless of their status.
   *
   * @return
   *   A structured array that looks like
   *   array($drupal_uid => array($repo_id => 'VCS username', ...), ...).
   *   If not a single account matches these constraints,
   *   an empty array is returned.
   */
  public final function getAccounts($constraints = array(), $include_unauthorized = FALSE) {
    $and_constraints = array();
    $params = array();

    // Filter by Drupal user id.
    if (isset($constraints['uids'])) {
      if (empty($constraints['uids'])) {
        return array();
      }
      $or_constraints = array();
      foreach ($constraints['uids'] as $uid) {
        $or_constraints[] = 'uid = %d';
        $params[] = $uid;
      }
      $and_constraints[] = '('. implode(' OR ', $or_constraints) .')';
    }

    // Filter by repository id.
    if (isset($constraints['repo_ids'])) {
      if (empty($constraints['repo_ids'])) {
        return array();
      }
      $or_constraints = array();
      foreach ($constraints['repo_ids'] as $repo_id) {
        $or_constraints[] = 'repo_id = %d';
        $params[] = $repo_id;
      }
      $and_constraints[] = '('. implode(' OR ', $or_constraints) .')';
    }

    // Filter by VCS username.
    if (isset($constraints['usernames'])) {
      if (empty($constraints['usernames'])) {
        return array();
      }
      $or_constraints = array();
      foreach ($constraints['usernames'] as $username) {
        $or_constraints[] = "username = '%s'";
        $params[] = $username;
      }
      $and_constraints[] = '('. implode(' OR ', $or_constraints) .')';
    }

    // Filter by usernames-by-repository.
    if (isset($constraints['usernames_by_repository'])) {
      if (empty($constraints['usernames_by_repository'])) {
        return array();
      }
      $or_constraints = array();
      foreach ($usernames_by_repository as $repo_id => $usernames) {
        $repo_constraint = 'repo_id = %d';
        $params[] = $repo_id;

        $username_constraints = array();
        foreach ($usernames as $username) {
          $username_constraints[] = "username = '%s'";
          $params[] = $username;
        }

        $or_constraints[] = '('. $repo_constraint
                            .' AND ('. implode(' OR ', $username_constraints) .'))';
      }
      $and_constraints[] = '('. implode(' OR ', $or_constraints) .')';
    }

    // All the constraints have been gathered, assemble them to a WHERE clause.
    $where = empty($and_constraints) ? '' : ' WHERE '. implode(' AND ', $and_constraints);

    // Execute the query.
    $result = db_query('SELECT uid, repo_id, username
                        FROM {versioncontrol_accounts}
                        '. $where .'
                        ORDER BY uid', $params);

    // Assemble the return value.
    $account_rows = array();
    $repo_ids = array();
    while ($account = db_fetch_object($result)) {
      $repo_ids[] = $account->repo_id;
      $account_rows[] = array('username' => $account->username, 'uid' => $account->uid, 'repo_id' => $account->repo_id);
    }
    if (empty($repo_ids)) {
      return array();
    }
    $repo_ids = array_unique($repo_ids);

    $repositories = VersioncontrolRepositoryCache::getInstance()->getRepositories(array('repo_ids' => $repo_ids));
    $accounts = array();

    foreach ($account_rows as $account_raw) {
      $repo = $repositories[$account_raw['repo_id']];
      $accountObj = new $repo->backend->classes['account']($account_raw['username'], $account_raw['uid'], $repo);
      // Only include approved accounts, except in case the caller said otherwise.
      if ($include_unauthorized
          || $accountObj->repository->isAccountAuthorized($accountObj->uid)) {
        if (!isset($accounts[$accountObj->uid])) {
          $accounts[$accountObj->uid] = array();
        }
        $accounts[$accountObj->uid][$accountObj->repository->repo_id] = $accountObj;
      }
      if (!$this->accountCache['uid'][$accountObj->uid][$accountObj->repository->repo_id] instanceof VersioncontrolAccount) {
        $this->cacheAccount($accountObj);
      }

    }
    return $accounts;
  }

  public function getAllAccounts() {
    if (!$this->allFetched) {
      $this->allFetched = TRUE;
      $repo_ids = array();
      $uids = array();
      foreach ($this->accountCache['uid'] as $uid => $accounts_per_repo) {
        foreach ($accounts_per_repo as $repo_id => $min_account) {
          if (!$min_account instanceof VersioncontrolAccount) {
            $repo_ids[] = $repo_id;
            $uids[] = $uid;
          }
        }
      }
      foreach ($this->getAccounts(array('repo_ids' => $repo_ids, 'uids' => $uids)) as $accounts_per_repo) {
        foreach ($accounts_per_repo as $repo_id => $account) {
          $this->cacheAccount($account);
        }
      }
    }
    return $this->accountCache['uid'];
  }

  private function cacheAccount(&$account) {
    $this->accountCache['uid'][$account->uid][$account->repository->repo_id] = &$account;
    $this->accountCache['repo_id'][$account->repository->repo_id][$account->uid] = &$account;
  }

}
