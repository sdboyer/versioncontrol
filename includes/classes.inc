<?php
// $Id$

/**
 * @file
 * Needed basic clases that are not entities.
 */

/**
 * Repository loader, sigleton class.
 */
final class VersioncontrolRepositoryCache {
  private static $instance;
  private $backends = array();
  private $repoCache = array('repo_id' => array(), 'name' => array(), 'vcs' => array());
  /**
   * Internal state variable indicating whether or not all repositories have
   * been fetched (via VersioncontrolRepositoryCache::getInstance()->getAllRepositories()).
   * @var bool
   */
  private $allFetched = FALSE;

  private function __construct() {
    // TODO really oughtta make this better.
    $this->backends = versioncontrol_get_backends();
    $result = db_query('SELECT repo_id, name, vcs FROM {versioncontrol_repositories}');
    // Cache a skeletal, low-mem overhead list of all the repos we have.
    while ($item = db_fetch_object($result)) {
      $this->repoCache['repo_id'][$item->repo_id] = &$item;
      // $this->repoCache['name'][$item->name] = &$item;
      // $this->repoCache['vcs'][$item->vcs] = &$item;
    }

    $this->repoCache['repo_ids'] = &$this->repoCache['repo_id']; // backwards compat
  }

  /**
   * Return the singleton's instance of the VersioncontrolRepositoryCache.
   *
   * @return VersioncontrolRepositoryCache
   */
  public static function getInstance() {
    if (!self::$instance instanceof VersioncontrolRepositoryCache) {
      self::$instance = new VersioncontrolRepositoryCache();
    }
    return self::$instance;
  }

  /**
   * Convenience function for retrieving one single repository by repository id
   * from cache.
   *
   * @return
   *   A single VersioncontroRepository array.
   *   If no repository corresponds to the given repository id, NULL is returned.
   */
  public function getRepository($repo_id) {
    if (!isset($this->repoCache['repo_id'][$repo_id])) {
      // no such repo. bail out.
      return;
    }
    if (!$this->repoCache['repo_id'][$repo_id] instanceof VersioncontrolRepository) {
      $this->cacheRepository(array_shift($this->getRepositories(array('repo_id' => $repo_id))));
    }
    return $this->repoCache['repo_id'][$repo_id];
  }

  /**
   * Retrieve a set of repositories that match the given constraints.
   *
   * @static
   * @param $constraints
   *   An optional array of constraints. Possible array elements are:
   *
   *   - 'vcs': An array of strings, like array('cvs', 'svn', 'git').
   *       If given, only repositories for these backends will be returned.
   *   - 'repo_ids': An array of repository ids.
   *       If given, only the corresponding repositories will be returned.
   *   - 'names': An array of repository names, like
   *       array('Drupal CVS', 'Experimental SVN'). If given,
   *       only repositories with these repository names will be returned.
   *
   * @return
   *   An array of repositories where the key of each element is the repository
   *   id. The corresponding value contains a VersioncontrolRepository object.
   *   If not a single repository matches these constraints,
   *   an empty array is returned.
   */
  public final function getRepositories($constraints = array()) {
    $auth_methods = versioncontrol_get_authorization_methods();

    if (isset($constraints['repo_ids'])) {
      $repo_ids = array();
      foreach ($constraints['repo_ids'] as $repo_id) {
        $repo_ids[] = (int) $repo_id;
      }
      $constraints['repo_ids'] = $repo_ids;
    }

    $constraints_serialized = serialize($constraints);
    if (isset($this->repoCache[$constraints_serialized])) {
      return $this->repoCache[$constraints_serialized];
    }

    list($and_constraints, $params) =
      _versioncontrol_construct_repository_constraints($constraints, $this->backends);

    // All the constraints have been gathered, assemble them to a WHERE clause.
    $where = empty($and_constraints) ? '' : ' WHERE '. implode(' AND ', $and_constraints);

    $result = db_query('SELECT * FROM {versioncontrol_repositories} r'. $where, $params);

    // Sort the retrieved repositories by backend.
    $repositories_by_backend = array();

    while ($repository = db_fetch_array($result)) {
      if (!isset($this->backends[$repository['vcs']])) {
        // don't include repositories for which no backend module exists
        continue;
      }

      if (!isset($repositories_by_backend[$repository['vcs']])) {
        $repositories_by_backend[$repository['vcs']] = array();
      }
      $repositories_by_backend[$repository['vcs']][$repository['repo_id']] = $repository;
    }

    // Add the fully assembled repositories to the result array.
    $result_repositories = array();
    foreach ($repositories_by_backend as $vcs => $vcs_repositories) {
      foreach ($vcs_repositories as $repository) {
        $vcs_repository = new $this->backends[$repository['vcs']]->classes['repo']($repository['repo_id'], $repository, FALSE);
        $vcs_repository->backend = $this->backends[$repository['vcs']];
        //TODO think how to improve this getter per repo, because it can cause
        //     performance problems, maybe it's better to use a per backend
        //     process(1-level-up foreach) but now it's not posible, because
        //     we can not inherit VersioncontrolRepositoryCache
        $vcs_repository->_getRepository();
        if (!$this->repoCache['repo_id'][$repository['repo_id']] instanceof VersioncontrolRepository) {
          $this->cacheRepository($vcs_repository);
        }
        $result_repositories[$repository['repo_id']] = $vcs_repository;
      }
    }


    $this->repoCache[$constraints_serialized] = $result_repositories; // cache the results
    return $result_repositories;
  }

  public function getAllRepositories() {
    if (!$this->allFetched) {
      $this->allFetched = TRUE;
      $repo_ids = array();
      foreach ($this->repoCache['repo_id'] as $repo_id => $min_repo) {
        if (!$min_repo instanceof VersioncontrolRepository) {
          $repo_ids[] = $repo_id;
        }
      }
      foreach ($this->getRepositories(array('repo_ids' => $repo_ids)) as $repo) {
        $this->cacheRepository($repo);
      }
    }
    return $this->repoCache['repo_id'];
  }

  private function cacheRepository(&$repository) {
    $this->repoCache['repo_id'][$repository->repo_id] = &$repository;
    $this->repoCache['name'][$repository->name][$repository->repo_id] = &$repository;
    $this->repoCache['vcs'][$repository->vcs][$repository->repo_id] = &$repository;
  }

}

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

/**
 * Operation loader, sigleton class.
 */
final class VersioncontrolOperationCache {
  private static $instance;

  /**
   * All possible operation constraints.
   * Each constraint is identified by its key which denotes the array key within
   * the $constraints parameter that is given to
   * VersioncontrolOperationCache::getInstance()->getOperations().
   * The array value of each element is a description array containing the
   * elements 'callback' and 'cardinality'.
   *
   */
  private static $constraint_info = array();

  //TODO decide cache keys criteria. this need further tought. As we
  //     are using object, references cost less than arrays, so more
  //     key should not affect that much(but need test). but huge places
  //     like d.o will have so many ops that we will not want vc_op_ids
  //     as prymary "cache indexer".
  private $opCache = array(
    'vc_op_id' => array(),
    'type' =>array(
      VERSIONCONTROL_OPERATION_COMMIT => array(),
      VERSIONCONTROL_OPERATION_BRANCH => array(),
      VERSIONCONTROL_OPERATION_TAG    => array(),
    ),
    //'repo_ids' => array(),
  );

  private function __construct() {
  /*
    $result = db_query('SELECT repo_id FROM {versioncontrol_repositories}');
    // Cache a skeletal, low-mem overhead list of all the repos we have.
    while ($repo = db_fetch_object($result)) {
      $this->opCache['repo_ids'][$repo->repo_id] = &$repo;
    }
   */
  }

  /**
   * Return the singleton's instance of the VersioncontrolOperationCache.
   *
   * @return VersioncontrolOperationCache
   */
  public static function getInstance() {
    if (!self::$instance instanceof VersioncontrolOperationCache) {
      self::$instance = new VersioncontrolOperationCache();
    }
    return self::$instance;
  }

  /**
   * Retrieve a set of commit, branch or tag operations that match the
   * given constraints.
   *
   * @param $constraints
   *   An optional array of constraints. Possible array elements are:
   *
   *   - 'vcs': An array of strings, like array('cvs', 'svn', 'git').
   *        If given, only operations for these backends will be
   *        returned.
   *   - 'repo_ids': An array of repository ids. If given, only
   *        operations for the corresponding repositories will be
   *        returned.
   *   - 'types': An array containing any combination of the three
   *        VERSIONCONTROL_OPERATION_{COMMIT,BRANCH,TAG} constants, like
   *        array(
   *          VERSIONCONTROL_OPERATION_COMMIT,
   *          VERSIONCONTROL_OPERATION_TAG
   *        ).
   *        If given, only operations of this type will be returned.
   *   - 'branches': An array of strings, like array('HEAD', 'DRUPAL-5').
   *        If given, only commits or branch operations on one of these
   *        branches will be returned.
   *   - 'tags': An array of strings, like
   *        array('DRUPAL-6-1', 'DRUPAL-6--1-0').
   *        If given, only tag operations with one of these tag names
   *        will be returned.
   *   - 'revisions': An array of strings, each containing a
   *        VCS-specific(global) revision, like '27491' for Subversion
   *        or some SHA-1 key in various distributed version control
   *        systems. If given, only operations with that revision
   *        identifier will be returned. Note that this constraint only
   *        works for version control systems that support global
   *        revision identifiers, so this will filter out all CVS
   *        operations.
   *   - 'labels': A combination of the 'branches' and 'tags'
            constraints.
   *   - 'paths': An array of strings (item locations), like
   *          array(
   *            '/trunk/contributions/modules/versioncontrol',
   *            '/trunk/contributions/themes/b2',
   *          ).
   *        If given, only operations affecting one of these items
   *        (or its children, in case the item is a directory) will be
   *        returned.
   *   - 'message': A string, or an array of strings (which will be
   *        combined with an "OR" operator). If given, only operations
   *        containing the string(s) in their log message will be
   *        returned.
   *   - 'item_revision_ids': An array of item revision ids. If given,
   *        only operations affecting one of the items with that id will
   *        be returned.
   *   - 'item_revisions': An array of strings, each containing a
   *        VCS-specific file-level revision, like '1.15.2.3' for CVS,
   *        '27491' for Subversion, or some SHA-1 key in various
   *        distributed version control systems.
   *        If given, only operations affecting one of the items with
   *        that item revision will be returned.
   *   - 'vc_op_ids': An array of operation ids. If given, only operations
   *        matching those ids will be returned.
   *   - 'date_lower': A Unix timestamp. If given, no operations will be
   *        retrieved that were performed earlier than this lower bound.
   *   - 'date_lower': A Unix timestamp. If given, no operations will be
   *        retrieved that were performed later than this upper bound.
   *   - 'uids': An array of Drupal user ids. If given, the result set
   *        will only contain operations that were performed by any of
   *        the specified users.
   *   - 'usernames': An array of system-specific usernames (the ones
   *        that the version control systems themselves get to see), like
   *        array('dww', 'jpetso'). If given, the result set will only
   *        contain operations that were performed by any of the
   *        specified users.
   *   - 'user_relation': If set to VERSIONCONTROL_USER_ASSOCIATED, only
   *        operations whose authors can be associated to Drupal users
   *        will be returned. If set to
   *        VERSIONCONTROL_USER_ASSOCIATED_ACTIVE, only users will be
   *        considered that are not blocked.
   *
   * @param $options
   *   An optional array of additional options for retrieving the
   *   operations.
   *   The following array keys are supported:
   *
   *   - 'query_type': If unset, the standard db_query() function is
   *        used to retrieve all operations that match the given
   *        constraints.
   *        Can be set to 'range' or 'pager' to use the db_query_range()
   *        or pager_query() functions instead. Additional options are
   *        required in this case.
   *   - 'count': Required if 'query_type' is either 'range' or 'pager'.
   *        Specifies the number of operations to be returned by this
   *        function.
   *   - 'from': Required if 'query_type' is 'range'. Specifies the first
   *        result row to return. (Usually you want to pass 0 for this
   *        one.)
   *   - 'pager_element': Optional for 'pager' as 'query_type'. An
   *        optional integer to distinguish between multiple pagers on
   *        one page.
   *
   * @return
   *   An array of operations, reversely sorted by the time of the
   *   operation.
   *   Each element contains an VersioncontrolOperation object with the
   *   'vc_op_id' identifier as key (which doesn't influence the
   *   sorting).
   *
   *   If not a single operation matches these constraints,
   *   an empty array is returned.
   */
  public function getOperations($constraints = array(), $options = array()) {
    $tables = array(
      'versioncontrol_operations' => array('alias' => 'op'),
      'versioncontrol_repositories' => array(
        'alias' => 'r',
        'join_on' => 'op.repo_id = r.repo_id',
      ),
    );
    // Construct the actual query, and let other modules provide "native"
    // custom constraints as well.
    $query_info = self::constructQuery(
      $constraints, $tables
    );
    if (empty($query_info)) {
      return array();
    }

    $query = 'SELECT DISTINCT(op.vc_op_id), op.type, op.date, op.uid,
      op.author, op.committer, op.message, op.revision, r.repo_id, r.vcs
      FROM '. $query_info['from'] .
      (empty($query_info['where']) ? '' : ' WHERE '. $query_info['where']) .'
      ORDER BY op.date DESC, op.vc_op_id DESC';

    $result = _versioncontrol_query($query, $query_info['params'], $options);

    $operations = array();
    $op_id_placeholders = array();
    $op_ids = array();
    $repo_ids = array();

    //TODO review on cache before query

    while ($row = db_fetch_object($result)) {
      // Remember which repositories and backends are being used for the
      // results of this query.
      if (!in_array($row->repo_id, $repo_ids)) {
        $repo_ids[] = $row->repo_id;
      }

      // Construct an operation array - nearly done already.
      // 'repo_id' is replaced by 'repository' further down
      $operations[$row->vc_op_id] = $row;
      $op_ids[] = $row->vc_op_id;
      $op_id_placeholders[] = '%d';
    }
    if (empty($operations)) {
      return array();
    }

    // Add the corresponding repository array to each operation.
    $repositories = VersioncontrolRepositoryCache::getInstance()->getRepositories(array('repo_ids' => $repo_ids));
    foreach ($operations as $vc_op_id => $operation) {
      $repo = $repositories[$operation->repo_id];
      $operationObj = new $repo->backend->classes['operation']($operation->type,
        $operation->committer, $operation->date, $operation->revision, $operation->message,
        $operation->author, $repo, $operation->vc_op_id);
      $operationObj->labels = array();
      $operationObj->uid = $operation->uid;
      $operations[$operation->vc_op_id] = $operationObj;
    }

    // Add the corresponding labels to each operation.
    $result = db_query('SELECT op.vc_op_id, oplabel.action,
      label.label_id, label.name, label.type
      FROM {versioncontrol_operations} op
      INNER JOIN {versioncontrol_operation_labels} oplabel
      ON op.vc_op_id = oplabel.vc_op_id
      INNER JOIN {versioncontrol_labels} label
      ON oplabel.label_id = label.label_id
      WHERE op.vc_op_id IN
      ('. implode(',', $op_id_placeholders) .')', $op_ids);

    while ($row = db_fetch_object($result)) {
      switch ($row->type) {
      case VERSIONCONTROL_LABEL_TAG:
        $operations[$row->vc_op_id]->labels[] = new VersioncontrolTag(
          $row->name, $row->action, $row->label_id,
          $operations[$row->vc_op_id]->repository
        );
        break;
      case VERSIONCONTROL_LABEL_BRANCH:
        $operations[$row->vc_op_id]->labels[] = new VersioncontrolBranch(
          $row->name, $row->action, $row->label_id,
          $operations[$row->vc_op_id]->repository
        );
        break;
      }
      $this->cacheOperation($operations[$row->vc_op_id]);
    }
    return $operations;
  }

  /**
   * Convenience function, calling getOperations() with a preset
   * of array(VERSIONCONTROL_OPERATION_COMMIT) for the 'types' constraint
   * (so only commits are returned). Parameters and result array are the same
   * as those from versioncontrol_get_operations().
   */
  public function getCommits($constraints = array(), $options = array()) {
    if (isset($constraints['types']) && !in_array(VERSIONCONTROL_OPERATION_COMMIT, $constraints['types'])) {
      return array(); // no commits in the original constraints, intersects to empty
    }
    $constraints['types'] = array(VERSIONCONTROL_OPERATION_COMMIT);
    return $this->getOperations($constraints, $options);
  }

  /**
   * Convenience function, calling VersioncontrolCache::getInstance()->_get_operations() with a preset
   * of array(VERSIONCONTROL_OPERATION_TAG) for the 'types' constraint
   * (so only tag operations or commits affecting emulated tags are returned).
   * Parameters and result array are the same as those
   * from versioncontrol_get_operations().
   *
   * @static
   */
  public static function getTags($constraints = array(), $options = array()) {
    if (isset($constraints['types']) && !in_array(VERSIONCONTROL_OPERATION_TAG, $constraints['types'])) {
      return array(); // no tags in the original constraints, intersects to empty
    }
    $constraints['types'] = array(VERSIONCONTROL_OPERATION_TAG);
    return $this->getOperations($constraints, $options);
  }

  /**
   * Convenience function, calling versioncontrol_get_operations() with a preset
   * of array(VERSIONCONTROL_OPERATION_BRANCH) for the 'types' constraint
   * (so only branch operations or commits affecting emulated branches
   * are returned). Parameters and result array are the same as those
   * from versioncontrol_get_operations().
   *
   * @static
   */
  public static function getBranches($constraints = array(), $options = array()) {
    if (isset($constraints['types']) && !in_array(VERSIONCONTROL_OPERATION_BRANCH, $constraints['types'])) {
      return array(); // no branches in the original constraints, intersects to empty
    }
    $constraints['types'] = array(VERSIONCONTROL_OPERATION_BRANCH);
    return $this->getOperations($constraints, $options);
  }

  private function cacheOperation(&$operation) {
    $this->opCache['vc_op_id'][$operation->vc_op_id] = &$operation;
    $this->opCache['type'][$operation->type] = &$operation;
    //$this->opCache['repo_ids'][$operation->repository->repo_id][$operation->vc_op_id] = &$operation;
  }

  /**
   * Retrieve the number of operations that match the given constraints,
   * plus some details about the first and last matching operation.
   *
   * @static
   * @param $constraints
   *   An optional array of constraints. This array has the same format
   *   as the one in versioncontrol_get_operations(), see the API
   *   documentation of that function for a detailed list of possible
   *   constraints.
   * @param $group_options
   *   An optional array of further options that change the returned
   *   value.  All of these are only used if the 'group_by' element is
   *   set.  The following array keys are recognized:
   *
   *   - 'group_by': If given, the result will be a list of statistics
   *        grouped by the given {versioncontrol_operations} columns
   *        instead of a single statistics object, with the grouping
   *        columns as array keys.  (In case multiple grouping columns
   *        are given, they will be concatenated with "\t" to make up the
   *        array key.) For example, if a non-grouped function call
   *        returned a single statistics object, a call specifying
   *        array('uid') for this option will return an array of multiple
   *        statistics objects with the Drupal user id as array key. You
   *        can also group by columns from other tables. In order to do
   *        that, an array needs to be passed instead of a simple column
   *        name, containing the keys 'table', 'column' and 'join
   *        callback' - the latter being a join callback like the ones in
   *        hook_versioncontrol_operation_constraint_info().
   *   - 'order_by': An array of columns to sort on. Allowed columns are
   *        'total_operations', 'first_operation_date',
   *        'last_operation_date' as well as any of the columns given in
   *        @p $group_by.
   *   - 'order_ascending': The default is to sort with DESC if sort
   *        columns are given, but ASC sorting will be used if this is
   *        set to TRUE.
   *   - 'query_type', 'count', 'from' and 'pager_element': Specifies
   *        different query types to execute and their associated
   *        options. The set of allowed values for these options is the
   *        same as in the $options array of
   *        versioncontrol_get_operations(), see the API documentation of
   *        that function for a detailed description.
   *
   * @return
   *   A statistics object with integers for the keys 'total_operations',
   *   'first_operation_date' and 'last_operation_date' (the latter two
   *   being Unix timestamps). If grouping columns were given, an array
   *   of such statistics objects is returned, with the grouping columns'
   *   values as additional properties for each object.
   *
   * @see VersioncontrolOperationCache::get_operations()
   */
  public static function getStatistics($constraints = array(), $group_options = array()) {
    $calculated_columns = array(
      'total_operations', 'first_operation_date', 'last_operation_date'
    );
    $tables = array(
      'versioncontrol_operations' => array('alias' => 'op'),
    );
    $qualified_group_by = array();

    // Resolve table aliases for the group-by and sort-by columns.
    if (!empty($group_options['group_by'])) {
      foreach ($group_options['group_by'] as &$column) {
        $table = is_string($column) ? 'versioncontrol_operations' : $column['table'];

        if (is_array($column)) {
          $table_callback = $column['join callback'];
          $table_callback($tables);
          $column = $column['column'];
        }
        $qualified_group_by[] = $tables[$table]['alias'] .'.'. $column;
      }
      if (!empty($group_options['order_by'])) {
        foreach ($group_options['order_by'] as &$column) {
          if (in_array($column, $calculated_columns)) {
            continue; // We don't want to prefix those with "op.".
          }
          $table = is_string($column) ? 'versioncontrol_operations' : $column['table'];
          $column = $tables[$table]['alias'] .'.'.
            (is_string($column) ? $column : $column['column']);
        }
      }
    }

    // Construct the actual query, and let other modules provide "native"
    // custom constraints as well.
    $query_info = self::constructQuery(
      $constraints, $tables
    );
    if (empty($query_info)) { // query won't yield any results
      return empty($group_options['group_by'])
        ? (object) array_fill_keys($calculated_columns, 0)
        : array();
    }

    $group_by_select = '';
    $group_by_clause = '';
    $order_by_clause = '';
    if (!empty($group_options['group_by'])) {
      $group_by_select = implode(', ', $qualified_group_by) .', ';
      $group_by_clause = ' GROUP BY '. implode(', ', $qualified_group_by);

      if (!empty($group_options['order_by'])) {
        $order_by_clause = ' ORDER BY '. implode(', ', $group_options['order_by'])
          . (empty($group_options['order_ascending']) ? ' DESC' : ' ASC');
      }
    }

    $query = '
      SELECT '. $group_by_select .'COUNT(op.vc_op_id) AS total_operations,
        MIN(op.date) AS first_operation_date, MAX(op.date) AS last_operation_date
        FROM '. $query_info['from'] .
        (empty($query_info['where']) ? '' : ' WHERE '. $query_info['where'])
        . $group_by_clause . $order_by_clause;

    // The query has been built, now execute it.
    $result = _versioncontrol_query($query, $query_info['params'], $group_options);
    $statistics = array();

    // Construct the result value.
    while ($row = db_fetch_object($result)) {
      if ($row->total_operations == 0) {
        $row->first_operation_date = 0;
        $row->last_operation_date = 0;
      }
      if (empty($group_options['group_by'])) {
        $statistics = $row;
        break; // Without grouping, it's just one result row anyways.
      }
      else {
        $group_values = array();
        foreach ($group_options['group_by'] as $column) {
          $group_values[$column] = $row->$column;
        }
        $key = implode("\t", $group_values);
        $statistics[$key] = $row;
      }
    }
    return $statistics;
  }

  /**
   * Assemble a list of query constraints from the given @p $constraints
   * and @p $tables arrays. Both of these are likely to be altered to
   * match the actual query, although in practice you probably won't need
   * them anymore.
   *
   * @return
   *   A query information array with keys 'from', 'where' and 'params',
   *   or an empty array if the constraints were invalid or will return
   *   an empty result set anyways. The 'from' and 'where' elements are
   *   strings to be used inside an SQL query (but don't include the
   *   actual FROM and WHERE keywords), and the 'params' element is an
   *   array with query parameter values for the returned WHERE clause.
   */
  private static function constructQuery(&$constraints, &$tables) {
    // Let modules alter the query by transforming custom constraints into
    // stuff that Version Control API can understand.
    drupal_alter('versioncontrol_operation_constraints', $constraints);

    $and_constraints = array();
    $params = array();
    $constraint_info = self::constraintInfo();
    $join_callbacks = array();

    foreach ($constraints as $key => $constraint_value) {
      if (!isset($constraint_info[$key])) {
        return array(); // No such constraint -> empty result.
      }

      // Standardization: put everything into an array if it isn't already.
      if ($constraint_info[$key]['cardinality'] == VERSIONCONTROL_CONSTRAINT_SINGLE) {
        $constraints[$key] = array($constraints[$key]);
      }
      elseif ($constraint_info[$key]['cardinality'] == VERSIONCONTROL_CONSTRAINT_SINGLE_OR_MULTIPLE && !is_array($constraint_value)) {
        $constraints[$key] = array($constraints[$key]);
      }

      if (empty($constraints[$key])) {
        return array(); // Empty set of constraint options -> empty result.
      }
      // Single-value constraints get the originally provided constraint value.
      // All others get the multiple-value constraint array.
      if ($constraint_info[$key]['cardinality'] == VERSIONCONTROL_CONSTRAINT_SINGLE) {
        $constraints[$key] = reset($constraints[$key]);
      }

      // If the constraint unconditionally requires extra tables, add them to
      // the $tables array by calling the join callback.
      if (!empty($constraint_info[$key]['join callback'])) {
        $function = $constraint_info[$key]['join callback'];

        if (!isset($join_callbacks[$function])) { // no need to call it twice
          $join_callbacks[$function] = TRUE;
          $function($tables);
        }
      }

      $function = $constraint_info[$key]['callback'];
      $function($constraints[$key], $tables, $and_constraints, $params);
    }

    // Now that we have all the information, let's construct some usable query parts.
    $from = array();
    foreach ($tables as $table_name => $table_info) {
      if (!empty($table_info['real_table'])) {
        $table_name = $table_info['real_table'];
      }
      $table_string = '{'. $table_name .'} '. $table_info['alias'];
      if (isset($table_info['join_on'])) {
        $table_string .= ' ON '. $table_info['join_on'] .' ';
      }
      $from[] = $table_string;
    }

    return array(
      'from' => implode(' INNER JOIN ', $from),
      'where' => '('. implode(' AND ', $and_constraints) .')',
      'params' => $params,
    );
  }

  /**
   * Gather a list of all possible operation constraints.
   *
   * Each constraint is identified by its key which denotes the array key
   * within the $constraints parameter that is given to getOperations().
   * The array value of each element is a description array containing
   * the elements 'callback' and 'cardinality'.
   */
  private static function constraintInfo() {
    if (empty(self::$constraint_info)) {
      foreach (module_implements('versioncontrol_operation_constraint_info') as $module) {
        $function = $module .'_versioncontrol_operation_constraint_info';
        $constraints = $function();

        foreach ($constraints as $key => $info) {
          self::$constraint_info[$key] = $info;
          if (!isset($info['callback'])) {
            self::$constraint_info[$key]['callback'] = $module .'_operation_constraint_'. $key;
          }
          if (!isset($info['cardinality'])) {
            self::$constraint_info[$key]['cardinality'] = VERSIONCONTROL_CONSTRAINT_MULTIPLE;
          }
        }
      }
    }
    return self::$constraint_info;
  }

}

interface VersioncontrolItemParallelItems {
  /**
   * Given an item in a repository, retrieve related versions of that
   * item on all different branches and/or tags where the item exists.
   *
   * @param $label_type_filter If unset, siblings will be retrieved both
   * on branches and tags.  If set to VERSIONCONTROL_LABEL_BRANCH or
   * VERSIONCONTROL_LABEL_TAG, results are limited to just that label
   * type.
   *
   * @return A structured item array of parallel items on all branches
   * and tags, possibly including the original item itself (if
   * appropriate for the given @p $label_type_filter). Array keys do not
   * convey any specific meaning, the corresponding values are again
   * structured arrays, each with a pair of 'item' and 'selected_label'
   * elements as follows.
   *
   *   - 'item': An item array, consisting of the following elements:
   *
   *        - 'type': Specifies the item type, which is either
   *        VERSIONCONTROL_ITEM_FILE or VERSIONCONTROL_ITEM_DIRECTORY for
   *        items that still exist, or VERSIONCONTROL_ITEM_FILE_DELETED
   *        respectively VERSIONCONTROL_ITEM_DIRECTORY_DELETED for items
   *        that have been removed.  - 'path': The path of the item at
   *        the specific revision.  - 'revision': The currently selected
   *        (file-level) revision of the item. If there is no such
   *        revision (which may be the case for directory items) then the
   *        'revision' element is an empty string.
   *
   *        If the returned item is already present in the database, the
   *        'item_revision_id' database identifier might also be filled
   *        in (optional, depends on the VCS backend).
   *
   *   - 'selected_label': A VersioncontrolLabel array describing the
   *   selected label.
   *
   *   NULL is returned if the given item is not inside the repository,
   *   or has not been inside the repository at the specified revision.
   *   An empty array is returned if the item is valid, but no parallel
   *   sibling items can be found for the given @p $label_type.
   */
  function _getParallelItems($label_type_filter = NULL);
}

interface VersioncontrolItemDirectoryContents {
  /**
   * Get this object directory contents.
   *
   * @param $recursive If FALSE, only the direct children of $path will
   * be retrieved.  If TRUE, you'll get every single descendant of $path.
   *
   * @return A structured item array of items that have been inside the
   * directory in its given state, including the directory item itself.
   * Array keys are the current/new paths. The corresponding values are
   * again structured arrays, each with a pair of 'item' and
   * 'selected_label' elements as follows.
   *
   *   - 'item': A VersioncontrolItem object
   *   - 'selected_label': In case no branch or tag applies to that item
   *   or could not be retrieved for whatever reasons, the selected label
   *   can also be NULL. Otherwise, it's a VersioncontrolLabel object.
   *
   *   NULL is returned if the given item is not under version control,
   *   or was not under version control at the time of the given
   *   revision.  The API module ensures that the passed item is a
   *   directory item.
   */
  function _getDirectoryContents($recursive = FALSE);
}

interface VersioncontrolItemExportFile {
  /**
   * Retrieve a copy of the contents of a given item in the repository.
   *
   * @param $destination The path where the copied file should be written
   * to.
   *
   * @return TRUE if the file was successfully created, FALSE if not.
   * The API module ensures that the passed item is a file item.
   */
  function _exportFile($destination);
}

interface VersioncontrolItemExportDirectory {
  /**
   * Retrieve a copy of the given directory item in the repository.
   *
   * @param $destination_dirpath
   *   The path of the directory that will receive the contents of the
   *   exported repository item. Version Control API makes sure that
   *   this directory does not exist when this function is called. (If
   *   it does exist, it will be deleted.) This directory will directly
   *   correspond to the item object - there are no artificial
   *   subdirectories, even if the @p $destination_dirpath has a
   *   different basename than the original path of item object.
   *
   * @return
   *   TRUE if successful, or FALSE if not.
   *   FALSE can be returned if the given item is not under version
   *   control, or was not under version control at the time of the
   *   given revision, or simply cannot be exported to the destination
   *   directory for any reason.
   */
  function _exportDirectory($destination_dirpath);
}

interface VersioncontrolItemGetFileAnnotation {
  /**
   * Retrieve an array where each element represents a single line of the
   * given file in the specified commit, annotated with the committer who
   * last modified that line. Note that annotations are generally a quite
   * slow operation, so expect this function to take a bit more time as
   * well.
   *
   * @return A structured array that consists of one element per line,
   * with line numbers as keys (starting from 1) and a structured array
   * as values, where each of them consists of elements with the
   * following keys:
   *
   *   - 'username': The system specific VCS username of the last
   *   committer.  - 'line': The contents of the line, without linebreak
   *   characters.
   *
   *   NULL is returned if the given item is not under version control,
   *   or was not under version control at the time of the given
   *   revision, or if it is marked as binary file.  The API module
   *   ensures that the passed item is a file item.
   */
  function _getFileAnnotation();
}

interface VersioncontrolRepositoryGetItem {

  /**
   * Try to retrieve a given item in a repository.
   *
   * @param $path
   *   The path of the requested item.
   * @param $constraints
   *   An optional array specifying one of two possible array keys which
   *   specify the exact revision of the item:
   *
   *   - 'revision': A specific revision for the requested item, in the
   *        same VCS-specific format as $item['revision']. A
   *        repository/path/revision combination is always unique, so no
   *        additional information is needed.
   *   - 'label': A label array with at least 'name' and 'type' elements
   *        filled in. If a label is provided, it should be incorporated
   *        into the result item as 'selected_label' (see return value
   *        docs), and will cause the most recent item on the label to
   *        be fetched. If the label includes an additional 'date'
   *        property holding a Unix timestamp, the item at that point of
   *        time will be retrieved instead of the most recent one. (For
   *        tag labels, there is only one item anyways, so nevermind the
   *        "most recent" part in that case.)
   *
   * @return
   *   If the item with the given path and revision cannot be retrieved,
   *   NULL is returned. Otherwise the result of the backend function is
   *   a structured array with the elements 'item' and 'selected_label',
   *   making up the whole picture.
   *
   *   - 'item': An item object.
   *
   *   - 'selected_label':
   *        In case no branch or tag applies to that item or could not be
   *        retrieved for whatever reasons, the selected label can also
   *        be NULL. Otherwise, it's a VersioncontrolLabel object
   *        describing the selected label.
   *
   *        In case the label array also contains the 'label_id' element
   *        (which happens when it's copied from the $operation->labels
   *        array) there will be a small performance improvement as the
   *        label doesn't need to be compared to and loaded from the
   *        database anymore.
   */
  public function _getItem($path, $constraints = array());

}

interface VersioncontrolRepositoryImportExport {

  /**
   * Import accounts into a repository, given text data from the accounts
   * file.
   * No accounts are deleted, new accounts are inserted, and existing
   * accounts are updated with imported ones.
   *
   * This function is optional for backend modules to implement. If it's not
   * implemented, the user will simply not be offered import functionality.
   *
   * @param $data
   *   The contents of the "account data" text area where the user has to
   *   enter/copy the contents of the version control system's accounts file.
   */
  public function importAccounts($data);

  /**
   * Export accounts of a repository to text data that is suitable for
   * copying to the version control system's accounts file.
   *
   * This function is optional for backend modules to implement. If it's
   * not implemented, the user will simply not be offered export
   * functionality.
   *
   * @param $accounts
   *   The list (array) of accounts that should be exported, given in the
   *   same format as the return value of
   *   VersioncontrolAccountCache::getInstance()->getAccounts().  All
   *   accounts in this list are from the above repository.
   *
   * @return
   *   The exported textual representation of the account list.
   */
  public function exportAccounts($accounts);

}
