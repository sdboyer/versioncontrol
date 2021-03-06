<?php
// $Id$
/**
 * @file
 * Repo class
 */

/**
 * Contain fundamental information about the repository.
 */
abstract class VersioncontrolRepository extends VersioncontrolEntity {
  // Attributes
  /**
   * db identifier
   *
   * @var    int
   */
  public $repo_id;

  /**
   * repository name inside drupal
   *
   * @var    string
   */
  public $name;

  /**
   * VCS string identifier
   *
   * @var    string
   */
  public $vcs;

  /**
   * where it is
   *
   * @var    string
   */
  public $root;

  /**
   * how ot authenticate
   *
   * @var    string
   */
  public $authorization_method = 'versioncontrol_admin';

  /**
   * An array of additional per-repository settings, mostly populated by
   * third-party modules. It is serialized on DB.
   */
  public $data = array();

  // Associations
  /**
   * The backend associated with this repository
   *
   * @var VersioncontrolBackend
   */
  public $backend;

  /**
   * An array of VersioncontrolEntityController objects used to spawn more
   * entities from this repository, if needed. These objects are lazy-
   * instanciated to avoid unnecessary object creation.
   *
   * @var array
   */
  protected $controllers = array();

  /**
   * Title callback for repository arrays.
   */
  public function titleCallback() {
    return check_plain($repository->name);
  }

  public function load($controller, $ids = array(), $conditions = array(), $options = array()) {
    if (!isset($this->controllers[$controller])) {
      $class = "Versioncontrol" . ucfirst($controller) . "Controller";
      $this->controllers[$controller] = new $class();
      $this->controllers[$controller]->setBackend($this->backend);
      // Set the controller to instanciate with this repository by default.
      $this->controllers[$controller]->defaultOptions['repository'] = $this;
    }
    $conditions['repo_id'] = $this->repo_id;
    return $this->controllers[$controller]->load($ids, $conditions, $options);
  }

  /**
   * Load known branches in a repository from the database as an array of
   * VersioncontrolBranch-descended objects.
   *
   * @param array $ids
   *   An array of branch ids. If given, only branches matching these ids will
   *   be returned.
   * @param array $conditions
   *   An associative array of additional conditions. These will be passed to
   *   the entity controller and composed into the query. The array should be
   *   key/value pairs with the field name as key, and desired field value as
   *   value. The value may also be an array, in which case the IN operator is
   *   used. For more complex requirements, FIXME finish!
   *   @see VersioncontrolEntityController::buildQuery() .
   *
   * @return
   *   An associative array of label objects, keyed on their
   */
  public function loadBranches($ids = array(), $conditions = array(), $options = array()) {
    return $this->load('branch', $ids, $conditions, $options);
  }

  /**
   * Load known tags in a repository from the database as an array of
   * VersioncontrolTag-descended objects.
   *
   * @param array $ids
   *   An array of tag ids. If given, only tags matching these ids will be
   *   returned.
   * @param array $conditions
   *   An associative array of additional conditions. These will be passed to
   *   the entity controller and composed into the query. The array should be
   *   key/value pairs with the field name as key, and desired field value as
   *   value. The value may also be an array, in which case the IN operator is
   *   used. For more complex requirements, FIXME finish!
   *   @see VersioncontrolEntityController::buildQuery() .
   *
   * @return
   *   An associative array of label objects, keyed on their
   */
  public function loadTags($ids = array(), $conditions = array(), $options = array()) {
    return $this->load('tag', $ids, $conditions, $options);
  }

  public function loadCommits($ids = array(), $conditions = array(), $options = array()) {
    $conditions['type'] = VERSIONCONTROL_OPERATION_COMMIT;
    return $this->load('operation', $ids, $conditions, $options);
  }

  public function loadAccounts($ids = array(), $conditions = array(), $options = array()) {
    return $this->load('account', $ids, $conditions, $options);
  }

  /**
   * Return TRUE if the account is authorized to commit in the actual
   * repository, or FALSE otherwise. Only call this function on existing
   * accounts or uid 0, the return value for all other
   * uid/repository combinations is undefined.
   *
   * @param $uid
   *   The user id of the checked account.
   */
  public function isAccountAuthorized($uid) {
    if (!$uid) {
      return FALSE;
    }
    $approved = array();

    foreach (module_implements('versioncontrol_is_account_authorized') as $module) {
      $function = $module .'_versioncontrol_is_account_authorized';

      // If at least one hook_versioncontrol_is_account_authorized()
      // returns FALSE, the account is assumed not to be approved.
      if ($function($this, $uid) === FALSE) {
        return FALSE;
      }
    }
    return TRUE;
  }

  public function save() {
    return isset($this->repo_id) ? $this->update() : $this->save();
  }

  public function buildSave(&$query) {

  }

  /**
   * Update a repository in the database, and call the necessary hooks.
   * The 'repo_id' and 'vcs' properties of the repository object must stay
   * the same as the ones given on repository creation,
   * whereas all other values may change.
   * TODO refactor to use a custom-built db_insert().
   */
  public final function update() {
    drupal_write_record('versioncontrol_repositories', $this, 'repo_id');

    $this->_update();

    // Everything's done, let the world know about it!
    module_invoke_all('versioncontrol_repository', 'update', $this);

    watchdog('special',
      'Version Control API: updated repository @repository',
      array('@repository' => $this->name),
      WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-repositories')
    );
  }

  protected function _update() {}

  /**
   * Insert a repository into the database, and call the necessary hooks.
   *
   * @return
   *   The finalized repository array, including the 'repo_id' element.
   */
  public final function insert() {
    if (isset($this->repo_id)) {
      // This is a new repository, it's not supposed to have a repo_id yet.
      unset($this->repo_id);
    }
    drupal_write_record('versioncontrol_repositories', $this);
    // drupal_write_record() has now added the 'repo_id' to the $repository array.

    $this->_insert();

    // Everything's done, let the world know about it!
    module_invoke_all('versioncontrol_repository', 'insert', $this);

    watchdog('special',
      'Version Control API: added repository @repository',
      array('@repository' => $this->name),
      WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-repositories')
    );
    return $this;
  }

  /**
   * Let child backend repo classes add information that _is not_ in
   * VersioncontrolRepository::data without modifying general flow if
   * necessary.
   */
  protected function _insert() {}

  /**
   * Delete a repository from the database, and call the necessary hooks.
   * Together with the repository, all associated commits and accounts are
   * deleted as well.
   */
  public final function delete() {
    // Delete operations.
    $branches = $this->loadBranches();
    foreach ($this->loadBranches as $branch) {
      $branch->delete();
    }
    foreach ($this->loadTags() as $tag) {
      $tag->delete();
    }
    foreach ($this->loadCommits() as $commit) {
      $commit->delete();
    }

    // $operations = VersioncontrolOperationCache::getInstance()->getOperations(array('repo_ids' => array($this->repo_id)));
    // foreach ($operations as $operation) {
    //  $operation->delete();
    // }
    // unset($operations); // conserve memory, this might get quite large

    // Delete labels.
    db_query('DELETE FROM {versioncontrol_labels}
              WHERE repo_id = %d', $this->repo_id);

    // Delete item revisions and related source item entries.
    $result = db_query('SELECT item_revision_id
                        FROM {versioncontrol_item_revisions}
                        WHERE repo_id = %d', $this->repo_id);
    $item_ids = array();
    $placeholders = array();

    while ($item_revision = db_fetch_object($result)) {
      $item_ids[] = $item_revision->item_revision_id;
      $placeholders[] = '%d';
    }
    if (!empty($item_ids)) {
      $placeholders = '('. implode(',', $placeholders) .')';

      db_query('DELETE FROM {versioncontrol_source_items}
                WHERE item_revision_id IN '. $placeholders, $item_ids);
      db_query('DELETE FROM {versioncontrol_source_items}
                WHERE source_item_revision_id IN '. $placeholders, $item_ids);
      db_query('DELETE FROM {versioncontrol_item_revisions}
                WHERE repo_id = %d', $this->repo_id);
    }
    unset($item_ids); // conserve memory, this might get quite large
    unset($placeholders); // ...likewise

    // Delete accounts.
    $accounts = $this->loadAccounts();
    foreach ($accounts as $uid => $usernames_by_repository) {
      foreach ($usernames_by_repository as $repo_id => $account) {
        $account->delete();
      }
    }

    // Announce deletion of the repository before anything has happened.
    module_invoke_all('versioncontrol_repository', 'delete', $this);

    $this->_delete();

    // Phew, everything's cleaned up. Finally, delete the repository.
    db_query('DELETE FROM {versioncontrol_repositories} WHERE repo_id = %d',
      $this->repo_id);

    watchdog('special',
      'Version Control API: deleted repository @repository',
      array('@repository' => $this->name),
      WATCHDOG_NOTICE, l('view', 'admin/project/versioncontrol-repositories')
    );
  }

  /**
   * Let child backend repo classes delete information that _is not_ in
   * VersioncontrolRepository::data without modifying general flow if
   * necessary.
   */
  protected function _delete() {}

  /**
   * Export a repository's authenticated accounts to the version control system's
   * password file format.
   *
   * FIXME this is TOTALLY broken, it calls itself.
   * @param $repository
   *   The repository array of the repository whose accounts should be exported.
   *
   * @return
   *   The plaintext result data which could be written into the password file
   *   as is.
   *
   */
  public function exportAccounts() {
    $accounts = $this->loadAccounts();
    return $repository->exportAccounts($accounts);
  }


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
   *   NULL is returned. Otherwise the result is an VersioncontrollItem
   *   object.
   */
  public function getItem($path, $constraints = array()) {
    if (!$this instanceof VersioncontrolRepositoryGetItem) {
      return NULL;
    }
    $info = $this->_getItem($path, $constraints);
    if (is_null($info)) {
      return NULL;
    }
    $item = $info['item'];
    $item['selected_label'] = new stdClass();
    $item['selected_label']->label = is_null($info['selected_label'])
      ? FALSE : $info['selected_label'];
    return $item;
  }

  /**
   * Get the user-visible version of a revision identifier (for an operation or
   * an item), as plaintext. By default, this function simply returns $revision.
   *
   * Version control backends can, however, choose to implement their own version
   * of this function, which for example makes it possible to cut the SHA-1 hash
   * in distributed version control systems down to a readable length.
   *
   * @param $revision
   *   The unformatted revision, as given in $operation->revision
   *   or $item->revision (or the respective table columns for those values).
   * @param $format
   *   Either 'full' for the original version, or 'short' for a more compact form.
   *   If the revision identifier doesn't need to be shortened, the results can
   *   be the same for both versions.
   */
  public function formatRevisionIdentifier($revision, $format = 'full') {
    return $revision;
  }

  /**
   * Convinience method to retrieve url handler.
   */
  public function getUrlHandler() {
    if (!isset($this->data['versioncontrol']['url_handler'])) {
      $this->data['versioncontrol']['url_handler'] =
        new VersioncontrolRepositoryUrlHandler(
          $this, VersioncontrolRepositoryUrlHandler::getEmpty()
        );
    }
    return $this->data['versioncontrol']['url_handler'];
  }

  /**
   * Retrieve the VCS username for a given Drupal user id in a specific
   * repository.
   *
   * @param $username
   *   The VCS specific username (a string) corresponding to the Drupal
   *   user.
   * @param $include_unauthorized
   *   If FALSE (which is the default), this function does not return
   *   accounts that are pending, queued, disabled, blocked, or otherwise
   *   non-approved. If TRUE, all accounts are returned, regardless of
   *   their status.
   *
   * @return
   *   The Drupal user id that corresponds to the given username and
   *   repository, or NULL if no Drupal user could be associated to
   *   those.
   */
  public function getAccountUidForUsername($username, $include_unauthorized = FALSE) {
    $result = db_query("SELECT uid, repo_id
      FROM {versioncontrol_accounts}
      WHERE username = '%s' AND repo_id = %d",
      $username, $this->repo_id);

    while ($account = db_fetch_object($result)) {
      // Only include approved accounts, except in case the caller said otherwise.
      if ($include_unauthorized || $this->isAccountAuthorized($account->uid)) {
        return $account->uid;
      }
    }
    return NULL;
  }

  /**
   * Retrieve the Drupal user id for a given VCS username in a specific
   * repository.
   *
   * @param $uid
   *   The Drupal user id corresponding to the VCS account.
   * @param $include_unauthorized
   *   If FALSE (which is the default), this function does not return
   *   accounts that are pending, queued, disabled, blocked, or otherwise
   *   non-approved. If TRUE, all accounts are returned, regardless of
   *   their status.
   *
   * @return
   *   The VCS username (a string) that corresponds to the given Drupal
   *   user and repository, or NULL if no VCS account could be associated
   *   to those.
   */
  function getAccountUsernameForUid($uid, $include_unauthorized = FALSE) {
    $result = db_query('SELECT uid, username, repo_id
      FROM {versioncontrol_accounts}
      WHERE uid = %d AND repo_id = %d',
      $uid, $this->repo_id);

    while ($account = db_fetch_object($result)) {
      // Only include approved accounts, except in case the caller said otherwise.
      if ($include_unauthorized || $this->isAccountAuthorized($account->uid)) {
        return $account->username;
      }
    }
    return NULL;
  }
}

/**
 * Contains the urls mainly for displaying.
 */
class VersioncontrolRepositoryUrlHandler {

  /**
   * Repository where this urls belongs.
   *
   * @var    VersioncontrolRepository
   */
  public $repository;

  /**
   * An array of repository viewer URLs.
   *
   * @var    array
   */
  public $urls;

  public function __construct($repository, $urls) {
    $this->repository = $repository;
    $this->urls = $urls;
  }

  /**
   * Explain and return and empty array of urls data member.
   */
  public static function getEmpty() {
    return array(
      /**
       * The URL of the repository viewer that displays a given commit in the
       * repository. "%revision" is used as placeholder for the
       * revision/commit/changeset identifier.
       */
      'commit_view'    => '',
      /**
       * The URL of the repository viewer that displays the commit log of a
       * given file in the repository. "%path" is used as placeholder for the
       * file path, "%revision" will be replaced by the file-level revision
       * (the one in {versioncontrol_item_revisions}.revision), and "%branch"
       * will be replaced by the branch name that the file is on.
       */
      'file_log_view'  => '',
      /**
       * The URL of the repository viewer that displays the contents of a given
       * file in the repository. "%path" is used as placeholder for the file
       * path, "%revision" will be replaced by the file-level revision (the one
       * in {versioncontrol_item_revisions}.revision), and "%branch" will be
       * replaced by the branch name that the file is on.
       */
      'file_view'      => '',
      /**
       * The URL of the repository viewer that displays the contents of a given
       * directory in the repository. "%path" is used as placeholder for the
       * directory path, "%revision" will be replaced by the file-level revision
       * (the one in {versioncontrol_item_revisions}.revision - only makes sense
       * if directories are versioned, of course), and "%branch" will be
       * replaced by the branch name that the directory is on.
       */
      'directory_view' => '',
      /**
       * The URL of the repository viewer that displays the diff between two
       * given files in the repository. "%path" and "%old-path" are used as
       * placeholders for the new and old paths (for some version control
       * systems, like CVS, those paths will always be the same).
       * "%new-revision" and "%old-revision" will be replaced by the
       * respective file-level revisions (from
       * {versioncontrol_item_revisions}.revision), and "%branch" will be
       * replaced by the branch name that the file is on.
       */
      'diff'           => '',
      /**
       * The URL of the issue tracker that displays the issue/case/bug page of
       * an issue id which presumably has been mentioned in a commit message.
       * As issue tracker URLs are likely specific to each repository, this is
       * also a per-repository setting. (Although... maybe it would make sense
       * to have per-project rather than per-repository. Oh well.)
       */
      'tracker'        => ''
    );
  }

  /**
   * Retrieve the URL of the repository viewer that displays the given commit
   * in the corresponding repository.
   *
   * @param $revision
   *   The revision on the commit operation whose view URL should be retrieved.
   *
   * @return
   *   The commit view URL corresponding to the given arguments.
   *   An empty string is returned if no commit view URL has been defined,
   *   or if the commit cannot be viewed for any reason.
   */
  public function getCommitViewUrl($revision) {
    if (empty($revision)) {
      return '';
    }
    return strtr($this->urls['commit_view'], array(
      '%revision' => $revision,
    ));
  }

  /**
   * Retrieve the URL of the repository viewer that displays the commit log
   * of the given item in the corresponding repository. If no such URL has been
   * specified by the user, the appropriate URL from the Commit Log module is
   * used as a fallback (if that module is enabled).
   *
   * @param $item
   *   The item whose log view URL should be retrieved.
   *
   * @return
   *   The item log view URL corresponding to the given arguments.
   *   An empty string is returned if no item log view URL has been defined
   *   (and if not even Commit Log is enabled), or if the item cannot be viewed
   *   for any reason.
   */
  public function getItemLogViewUrl(&$item) {
    $label = $item->getSelectedLabel();

    if (isset($label->type) && $label->type == VERSIONCONTROL_LABEL_BRANCH) {
      $current_branch = $label['name'];
    }

    if (!empty($this->urls['file_log_view'])) {
      if ($item->isFile()) {
        return strtr($this->urls['file_log_view'], array(
          '%path'     => $item->path,
          '%revision' => $item->revision,
          '%branch'   => isset($current_branch) ? $current_branch : '',
        ));
      }
      // The default URL backend doesn't do log view URLs for directory items:
      return '';
    }
    elseif (module_exists('commitlog')) { // fallback, as 'file_log_view' is empty
      $query = array(
        'repos' => $item->repository->repo_id,
        'paths' => drupal_urlencode($item->path),
      );
      if (isset($current_branch)) {
        $query['branches'] = $current_branch;
      }
      return url('commitlog', array(
        'query' => $query,
        'absolute' => TRUE,
      ));
    }
    return ''; // in case we really can't retrieve any sensible URL
  }

  /**
   * Retrieve the URL of the repository viewer that displays the contents of the
   * given item in the corresponding repository.
   *
   * @param $item
   *   The item whose view URL should be retrieved.
   *
   * @return
   *   The item view URL corresponding to the given arguments.
   *   An empty string is returned if no item view URL has been defined,
   *   or if the item cannot be viewed for any reason.
   */
  public function getItemViewUrl(&$item) {
    $label = $item->getSelectedLabel();

    if (isset($label->type) && $label->type == VERSIONCONTROL_LABEL_BRANCH) {
      $current_branch = $label->name;
    }
    $view_url = $item->isFile()
      ? $this->urls['file_view']
      : $this->urls['directory_view'];

    return strtr($view_url, array(
      '%path'     => $item['path'],
      '%revision' => $item['revision'],
      '%branch'   => isset($current_branch) ? $current_branch : '',
    ));
  }

  /**
   * Retrieve the URL of the repository viewer that displays the diff between
   * two given files in the corresponding repository.
   *
   * @param $file_item_new
   *   The new version of the file that should be diffed.
   * @param $file_item_old
   *   The old version of the file that should be diffed.
   *
   * @return
   *   The diff URL corresponding to the given arguments.
   *   An empty string is returned if no diff URL has been defined,
   *   or if the two items cannot be diffed for any reason.
   */
  public function getDiffUrl(&$file_item_new, $file_item_old) {
    $label = $file_item_new->getSelectedLabel();

    if (isset($label['type']) && $label['type'] == VERSIONCONTROL_LABEL_BRANCH) {
      $current_branch = $label['name'];
    }
    return strtr($this->urls['diff'], array(
      '%path'         => $file_item_new['path'],
      '%new-revision' => $file_item_new['revision'],
      '%old-path'     => $file_item_old['path'],
      '%old-revision' => $file_item_old['revision'],
      '%branch'       => isset($current_branch) ? $current_branch : '',
    ));
  }

  /**
   * Retrieve the URL of the issue tracker that displays the issue/case/bug page
   * of an issue id which presumably has been mentioned in a commit message.
   * As issue tracker URLs are specific to each repository, this also needs
   * to be given as argument.
   *
   * @param $issue_id
   *   A number that uniquely identifies the mentioned issue/case/bug.
   *
   * @return
   *   The issue tracker URL corresponding to the given arguments.
   *   An empty string is returned if no issue tracker URL has been defined.
   */
  public function getTrackerUrl($issue_id) {
    return strtr($this->urls['tracker'], array('%d' => $issue_id));
  }

}
