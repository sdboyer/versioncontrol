<?php
// $Id$

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
