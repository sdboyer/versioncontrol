<?php
// $Id$
/**
 * @file
 * Operation class
 */

require_once 'VersioncontrolItem.php';
require_once 'VersioncontrolBranch.php';
require_once 'VersioncontrolTag.php';

/**
 * @name VCS operations
 * a.k.a. stuff that is recorded for display purposes.
 */
//@{
define('VERSIONCONTROL_OPERATION_COMMIT', 1);
define('VERSIONCONTROL_OPERATION_BRANCH', 2);
define('VERSIONCONTROL_OPERATION_TAG',    3);
//@}

/**
 * Stuff that happened in a repository at a specific time
 */
abstract class VersioncontrolOperation implements ArrayAccess {
  /**
   * db identifier
   *
   * The Drupal-specific operation identifier (a simple integer)
   * which is unique among all operations (commits, branch ops, tag ops)
   * in all repositories.
   *
   * @var    int
   */
  public $vc_op_id;

  /**
   * Who actually perform the change on the repository.
   *
   * @var    string
   */
  public $committer;

  /**
   * The time when the operation was performed, given as
   * Unix timestamp. (For commits, this is the time when the revision
   * was committed, whereas for branch/tag operations it is the time
   * when the files were branched or tagged.)
   *
   * @var    timestamp
   */
  public $date;

  /**
   * The VCS specific repository-wide revision identifier,
   * like '' in CVS, '27491' in Subversion or some SHA-1 key in various
   * distributed version control systems. If there is no such revision
   * (which may be the case for version control systems that don't support
   * atomic commits) then the 'revision' element is an empty string.
   * For branch and tag operations, this element indicates the
   * (repository-wide) revision of the files that were branched or tagged.
   *
   * @var    string
   */
  public $revision;

  /**
   * The log message for the commit, tag or branch operation.
   * If a version control system doesn't support messages for the current
   * operation type, this element should be empty.
   *
   * @var    string
   */
  public $message;

  /**
   * The system specific VCS username of the user who executed this
   * operation(aka who write the change)
   *
   * @var    string
   */
  public $author;

  /**
   * The repository where this operation occurs.
   *
   * @var    VersioncontrolRepository
   */
  public $repository;

  /**
   * The type of the operation - one of the
   * VERSIONCONTROL_OPERATION_{COMMIT,BRANCH,TAG} constants.
   *
   * @var    string
   */
  public $type;

  /**
   * An array of branches or tags that were affected by this
   * operation. Branch and tag operations are known to only affect one
   * branch or tag, so for these there will be only one element (with 0
   * as key) in 'labels'. Commits might affect any number of branches,
   * including none. Commits that emulate branches and/or tags (like
   * in Subversion, where they're not a native concept) can also include
   * add/delete/move operations for labels, as detailed below.
   * Mind that the main development branch - e.g. 'HEAD', 'trunk'
   * or 'master' - is also considered a branch. Each element in 'labels'
   * is a VersioncontrolLabel(VersioncontrolBranch VersioncontrolTag)
   *
   * @var    array
   */
  public $labels;

  /**
   * The Drupal user id of the operation author, or 0 if no Drupal user
   * could be associated to the author.
   *
   * @var    int
   */
  public $uid;

  /**
   * Error messages used mainly to get descriptions of errors at
   * hasWriteAccess().
   */
  private static $error_messages = array();

  /**
   * Constructor
   */
  public function __construct($type, $committer, $date, $revision, $message, $author = NULL, $repository = NULL, $vc_op_id = NULL) {
    $this->type = $type;
    $this->committer = $committer;
    $this->date = $date;
    $this->revision = $revision;
    $this->message = $message;
    $this->author = (is_null($author))? $committer: $author;
    $this->repository = $repository;
    $this->vc_op_id = $vc_op_id;
  }

  /**
   * Retrieve all items that were affected by an operation.
   *
   * @param $fetch_source_items
   *   If TRUE, source and replaced items will be retrieved as well,
   *   and stored as additional properties inside each item array.
   *   If FALSE, only current/new items will be retrieved.
   *   If NULL (default), source and replaced items will be retrieved for commits
   *   but not for branch or tag operations.
   *
   * @return
   *   A structured array containing all items that were affected by the given
   *   operation. Array keys are the current/new paths, even if the item doesn't
   *   exist anymore (as is the case with delete actions in commits).
   *   The associated array elements are structured item arrays and consist of
   *   the following elements:
   *
   *   - 'type': Specifies the item type, which is either
   *        VERSIONCONTROL_ITEM_FILE or VERSIONCONTROL_ITEM_DIRECTORY for items
   *        that still exist, or VERSIONCONTROL_ITEM_FILE_DELETED respectively
   *        VERSIONCONTROL_ITEM_DIRECTORY_DELETED for items that have been
   *        removed (by a commit's delete action).
   *   - 'path': The path of the item at the specific revision.
   *   - 'revision': The (file-level) revision when the item was changed.
   *        If there is no such revision (which may be the case for
   *        directory items) then the 'revision' element is an empty string.
   *   - 'item_revision_id': Identifier of this item revision in the database.
   *        Note that you can only rely on this element to exist for
   *        operation items - functions that interface directly with the VCS
   *        (such as VersioncontrolItem::getDirectoryContents() or
   *        VersioncontrolItem::getParallelItems()) might not include
   *        this identifier, for obvious reasons.
   *
   *   If the @p $fetch_source_items parameter is TRUE,
   *   versioncontrol_fetch_source_items() will be called on the list of items
   *   in order to retrieve additional information about their origin.
   *   The following elements will be set for each item in addition
   *   to the ones listed above:
   *
   *   - 'action': Specifies how the item was changed.
   *        One of the predefined VERSIONCONTROL_ACTION_* values.
   *   - 'source_items': An array with the previous revision(s) of the affected
   *        item. Empty if 'action' is VERSIONCONTROL_ACTION_ADDED. The key for
   *        all items in this array is the respective item path.
   *   - 'replaced_item': The previous but technically unrelated item at the
   *        same location as the current item. Only exists if this previous item
   *        was deleted and replaced by a different one that was just moved
   *        or copied to this location.
   *   - 'line_changes': Only exists if line changes have been recorded for this
   *        action - if so, this is an array containing the number of added lines
   *        in an element with key 'added', and the number of removed lines in
   *        the 'removed' key.
   * FIXME refactor me to oo
   */
  public function getItems($fetch_source_items = NULL) {
    $items = array();
    $result = db_query(
      'SELECT ir.item_revision_id, ir.path, ir.revision, ir.type
      FROM {versioncontrol_operation_items} opitem
      INNER JOIN {versioncontrol_item_revisions} ir
      ON opitem.item_revision_id = ir.item_revision_id
      WHERE opitem.vc_op_id = %d AND opitem.type = %d',
      $this->vc_op_id, VERSIONCONTROL_OPERATION_MEMBER_ITEM);

    while ($item_revision = db_fetch_object($result)) {
      $items[$item_revision->path] = new $this->repository->backend->classes['item']($item_revision->type, $item_revision->path, $item_revision->revision, NULL, $this->repository, NULL, $item_revision->item_revision_id);
      $items[$item_revision->path]->selected_label = new stdClass();
      $items[$item_revision->path]->selected_label->get_from = 'operation';
      $items[$item_revision->path]->selected_label->operation = &$this;

      //TODO inherit from operation class insteadof types?
      if ($this->type == VERSIONCONTROL_OPERATION_COMMIT) {
        $items[$item_revision->path]->commit_operation = $this;
      }
    }

    if (!isset($fetch_source_items)) {
      // By default, fetch source items for commits but not for branch or tag ops.
      $fetch_source_items = ($this->type == VERSIONCONTROL_OPERATION_COMMIT);
    }
    if ($fetch_source_items) {
      versioncontrol_fetch_source_items($this->repository, $items);
    }
    ksort($items); // similar paths should be next to each other
    return $items;
  }

  /**
   * Replace the set of affected labels of the actual object with the one in
   * @p $labels. If any of the given labels does not yet exist in the
   * database, a database entry (including new 'label_id' array element) will
   * be written as well.
   */
  public function updateLabels($labels) {
    module_invoke_all('versioncontrol_operation_labels',
      'update', $this, $labels
    );
    $this->setLabels($labels);
  }

  /**
   * Insert a commit, branch or tag operation into the database, and call the
   * necessary module hooks. Only call this function after the operation has been
   * successfully executed.
   *
   * @param $operation_items
   *   A structured array containing the exact details of happened to each
   *   item in this operation. The structure of this array is the same as
   *   the return value of VersioncontrolOperation::getItems() - that is,
   *   elements for 'type', 'path' and 'revision' - but doesn't include the
   *   'item_revision_id' element, that one will be filled in by this function.
   *
   *   For commit operations, you also have to fill in the 'action' and
   *   'source_items' elements (and optionally 'replaced_item') that are also
   *   described in the VersioncontrolOperation::getItems() API documentation.
   *   The 'line_changes' element, as in VersioncontrolOperation::getItems(),
   *   is optional to provide.
   *
   *   This parameter is passed by reference as the insert operation will
   *   check the validity of a few item properties and will also assign an
   *   'item_revision_id' property to each of the given items. So when this
   *   function returns with a result other than NULL, the @p $operation_items
   *   array will also be up to snuff for further processing.
   *
   * @return
   *   The finalized operation array, with all of the 'vc_op_id', 'repository'
   *   and 'uid' properties filled in, and 'repo_id' removed if it existed before.
   *   Labels are now equipped with an additional 'label_id' property.
   *   (For more info on these labels, see the API documentation for
   *   versioncontrol_get_operations() and VersioncontrolOperation::getItems().)
   *   In case of an error, NULL is returned instead of the operation array.
   */
  public final function insert(&$operation_items) {
    $this->fill(TRUE);

    if (!isset($this->repository)) {
      return NULL;
    }

    // Ok, everything's there, insert the operation into the database.
    $this->repo_id = $this->repository->repo_id; // for drupal_write_record()
    //FIXME $this->uid = 0;
    drupal_write_record('versioncontrol_operations', $this);
    unset($this->repo_id);
    // drupal_write_record() has now added the 'vc_op_id' to the $operation array.

    // Insert labels that are attached to the operation.
    $this->setLabels($this->labels);

    $vcs = $this->repository->vcs;

    // So much for the operation itself, now the more verbose part: items.
    ksort($operation_items); // similar paths should be next to each other

    foreach ($operation_items as $path => $item) {
      $item->sanitize();
      $item->ensure();
      $this->insertOperationItem($item,
        VERSIONCONTROL_OPERATION_MEMBER_ITEM);
      $item['selected_label'] = new stdClass();
      $item['selected_label']->get_from = 'operation';
      $item['selected_label']->successor_item = &$this;

      // If we've got source items (which is the case for commit operations),
      // add them to the item revisions and source revisions tables as well.
      foreach ($item->source_items as $key => $source_item) {
        $source_item->ensure();
        $item->insertSourceRevision($source_item, $item->action);

        // Cache other important items in the operations table for 'path' search
        // queries, because joining the source revisions table is too expensive.
        switch ($item['action']) {
        case VERSIONCONTROL_ACTION_MOVED:
        case VERSIONCONTROL_ACTION_COPIED:
        case VERSIONCONTROL_ACTION_MERGED:
          if ($item->path != $source_item->path) {
            $this->insertOperationItem($source_item,
              VERSIONCONTROL_OPERATION_CACHED_AFFECTED_ITEM);
          }
          break;
        default: // No additional caching for added, modified or deleted items.
          break;
        }

        $source_item->selected_label = new stdClass();
        $source_item->selected_label->get_from = 'other_item';
        $source_item->selected_label->other_item = &$item;
        $source_item->selected_label->other_item_tags = array('successor_item');

        $item->source_items[$key] = $source_item;
      }
      // Plus a special case for the "added" action, as it needs an entry in the
      // source items table but contains no items in the 'source_items' property.
      if ($item->action == VERSIONCONTROL_ACTION_ADDED) {
        $item->insertSourceRevision(0, $item['action']);
      }

      // If we've got a replaced item (might happen for copy/move commits),
      // add it to the item revisions and source revisions table as well.
      if (isset($item->replaced_item)) {
        $item->replaced_item->ensure();
        $item->insertSourceRevision($item->replaced_item,
          VERSIONCONTROL_ACTION_REPLACED);
        $item->replaced_item->selected_label = new stdClass();
        $item->replaced_item->selected_label->get_from = 'other_item';
        $item->replaced_item->selected_label->other_item = &$item;
        $item->replaced_item->selected_label->other_item_tags = array('successor_item');
      }
      $operation_items[$path] = $item;
    }

    // Notify the backend first.
    $this->_insert($operation_items);

    // Everything's done, let the world know about it!
    module_invoke_all('versioncontrol_operation',
      'insert', $this, $operation_items
    );

    // This one too, as there is also an update function & hook for it.
    // Pretend that the labels didn't exist beforehand.
    $labels = $this->labels;
    $this->labels = array();
    module_invoke_all('versioncontrol_operation_labels',
      'insert', $this, $labels
    );
    $this->labels = $labels;

    // Rules integration, because we like to enable people to be flexible.
    // FIXME change callback
    if (module_exists('rules')) {
      rules_invoke_event('versioncontrol_operation_insert', array(
        'operation' => $this,
        'items' => $operation_items,
      ));
    }

    //FIXME avoid return, it's on the object
    return $this;
  }

  /**
   * Let child backend operation classes add information if necessary.
   */
  protected function _insert($operation_items) {
  }

  /**
   * Delete a commit, a branch operation or a tag operation from the database,
   * and call the necessary hooks.
   *
   * @param $operation
   *   The commit, branch operation or tag operation array containing
   *   the operation that should be deleted.
   */
  public final function delete() {
    $operation_items = $this->getItems();

    // As versioncontrol_update_operation_labels() provides an update hook for
    // operation labels, we should also have a delete hook for completeness.
    module_invoke_all('versioncontrol_operation_labels',
      'delete', $this, array());
    // Announce deletion of the operation before anything has happened.
    // Calls hook_versioncontrol_commit(), hook_versioncontrol_branch_operation()
    // or hook_versioncontrol_tag_operation().
    module_invoke_all('versioncontrol_operation',
      'delete', $this, $operation_items);

    // Provide an opportunity for the backend to delete its own stuff.
    $this->_delete($operation_items);

    db_query('DELETE FROM {versioncontrol_operation_labels}
    WHERE vc_op_id = %d', $this->vc_op_id);
    db_query('DELETE FROM {versioncontrol_operation_items}
    WHERE vc_op_id = %d', $this->vc_op_id);
    db_query('DELETE FROM {versioncontrol_operations}
    WHERE vc_op_id = %d', $this->vc_op_id);
  }

  /**
   * Let child backend repo classes add information that _is not_ in
   * VersioncontrolRepository::data without modifying general flow if
   * necessary.
   */
  protected function _delete($operation_items) {
  }

  /**
   * Fill in various operation members into the object(commit, branch op or tag
   * op), in case those values are not given.
   *
   * @param $operation
   *   The plain operation array that might lack have some properties yet.
   * @param $include_unauthorized
   *   If FALSE, the 'uid' property will receive a value of 0 for known
   *   but unauthorized users. If TRUE, all known users are mapped to their uid.
   */
  private function fill($include_unauthorized = FALSE) {
    // If not already there, retrieve the full repository object.
    // FIXME: take one always set member, not sure if root is one | set other condition here
    if (!isset($this->repository->root) && isset($this->repo_id)) {
      $this->repository = VersioncontrolRepositoryCache::getInstance()->getRepository($this->repository->repo_id);
      unset($this->repo_id);
    }

    // If not already there, retrieve the Drupal user id of the committer.
    if (!isset($this->author)) {
      $uid = $this->repository->getAccountUidForUsername(
        $this->author, $include_unauthorized
      );
      // If no uid could be retrieved, blame the commit on user 0 (anonymous).
      $this->author = isset($this->author) ? $this->author : 0;
    }

    // For insertions (which have 'date' set, as opposed to write access checks),
    // fill in the log message if it's unset. We don't want to do this for
    // write access checks because empty messages are denied access,
    // which requires distinguishing between unset and empty.
    if (isset($this->date) && !isset($this->message)) {
      $this->message = '';
    }
  }

  /**
   * Retrieve the list of access errors.
   *
   * If versioncontrol_has_commit_access(), versioncontrol_has_branch_access()
   * or versioncontrol_has_tag_access() returned FALSE, you can use this function
   * to retrieve the list of error messages from the various access checks.
   * The error messages do not include trailing linebreaks, it is expected that
   * those are inserted by the caller.
   */
  private function getAccessErrors() {
    return self::$error_messages;
  }

  /**
   * Set the list of access errors.
   */
  private function setAccessErrors($new_messages) {
    if (isset($new_messages)) {
      self::$error_messages = $new_messages;
    }
  }

  /**
   * Write @p $labels to the database as set of affected labels of the
   * actual operation object. Label ids are not required to exist yet.
   * After this the set of labels, all of them with 'label_id' filled in.
   *
   * @return
   */
  private function setLabels($labels) {
    db_query("DELETE FROM {versioncontrol_operation_labels}
    WHERE vc_op_id = %d", $this->vc_op_id);

    foreach ($labels as &$label) {
      $label->ensure();
      db_query("INSERT INTO {versioncontrol_operation_labels}
      (vc_op_id, label_id, action) VALUES (%d, %d, %d)",
        $this->vc_op_id, $label->label_id, $label->action);
    }
    $this->labels = $labels;
  }

  /**
   * Insert an operation item entry into the {versioncontrol_operation_items} table.
   * The item is expected to have an 'item_revision_id' property already.
   */
  private function insertOperationItem($item, $type) {
    // Before inserting that item entry, make sure it doesn't exist already.
    db_query("DELETE FROM {versioncontrol_operation_items}
    WHERE vc_op_id = %d AND item_revision_id = %d",
    $this->vc_op_id, $item->item_revision_id);

    db_query("INSERT INTO {versioncontrol_operation_items}
    (vc_op_id, item_revision_id, type) VALUES (%d, %d, %d)",
      $this->vc_op_id, $item->item_revision_id, $type);
  }

  /**
   * Determine if a commit, branch or tag operation may be executed or not.
   * Call this function inside a pre-commit hook.
   *
   * @param $operation
   *   A single operation array like the ones returned by
   *   versioncontrol_get_operations(), but leaving out on a few details that
   *   will instead be determined by this function. This array describes
   *   the operation that is about to happen. Here's the allowed elements:
   *
   *   - 'type': The type of the operation - one of the
   *        VERSIONCONTROL_OPERATION_{COMMIT,BRANCH,TAG} constants.
   *   - 'repository': The repository where this operation occurs,
   *        given as a structured array, like the return value
   *        of versioncontrol_get_repository().
   *        You can either pass this or 'repo_id'.
   *   - 'repo_id': The repository where this operation occurs, given as a simple
   *        integer id. You can either pass this or 'repository'.
   *   - 'uid': The Drupal user id of the committer. Passing this is optional -
   *        if it isn't set, this function will determine the uid.
   *   - 'username': The system specific VCS username of the committer.
   *   - 'message': The log message for the commit, tag or branch operation.
   *        If a version control system doesn't support messages for the current
   *        operation type, this element must not be set. Operations with
   *        log messages that are set but empty will be denied access.
   *
   *   - 'labels': An array of branches or tags that will be affected by this
   *        operation. Branch and tag operations are known to only affect one
   *        branch or tag, so for these there will be only one element (with 0
   *        as key) in 'labels'. Commits might affect any number of branches,
   *        including none. Commits that emulate branches and/or tags (like
   *        in Subversion, where they're not a native concept) can also include
   *        add/delete/move operations for labels, as detailed below.
   *        Mind that the main development branch - e.g. 'HEAD', 'trunk'
   *        or 'master' - is also considered a branch. Each element in 'labels'
   *        is a VersioncontrolLabel(VersioncontrolBranch VersioncontrolTag)
   *
   * @param $operation_items
   *   A structured array containing the exact details of what is about to happen
   *   to each item in this commit. The structure of this array is the same as
   *   the return value of VersioncontrolOperation::getItems() - that is,
   *   elements for 'type', 'path', 'revision', 'action', 'source_items' and
   *   'replaced_item' - but doesn't include the 'item_revision_id' element as
   *   there's no relation to the database yet.
   *
   *   The 'action', 'source_items', 'replaced_item' and 'revision' elements
   *   of each item are optional and may be left unset.
   *
   * @return
   *   TRUE if the operation may happen, or FALSE if not.
   *   If FALSE is returned, you can retrieve the concerning error messages
   *   by calling versioncontrol_get_access_errors().
   */
  protected function hasWriteAccess($operation, $operation_items) {
    $operation->fill();

    // If we can't determine this operation's repository,
    // we can't really allow the operation in the first place.
    if (!isset($operation['repository'])) {
      switch ($operation['type']) {
      case VERSIONCONTROL_OPERATION_COMMIT:
        $type = t('commit');
        break;
      case VERSIONCONTROL_OPERATION_BRANCH:
        $type = t('branch');
        break;
      case VERSIONCONTROL_OPERATION_TAG:
        $type = t('tag');
        break;
      }
      $this->setAccessErrors(array(t(
        '** ERROR: Version Control API cannot determine a repository
        ** for the !commit-branch-or-tag information given by the VCS backend.',
        array('!commit-branch-or-tag' => $type)
      )));
      return FALSE;
    }

    // If the user doesn't have commit access at all, we can't allow this as well.
    $repo_data = $operation->repository->data['versioncontrol'];

    if (!$repo_data['allow_unauthorized_access']) {

      if (!$operation->repository->isAccountAuthorized($operation->uid)) {
        $this->setAccessErrors(array(t(
          '** ERROR: !user does not have commit access to this repository.',
          array('!user' => $operation->committer)
        )));
        return FALSE;
      }
    }

    // Don't let people do empty log messages, that's as evil as it gets.
    if (isset($operation['message']) && empty($operation['message'])) {
      $this->setAccessErrors(array(
        t('** ERROR: You have to provide a log message.'),
      ));
      return FALSE;
    }

    // Also see if other modules have any objections.
    $error_messages = array();

    foreach (module_implements('versioncontrol_write_access') as $module) {
      $function = $module .'_versioncontrol_write_access';

      // If at least one hook_versioncontrol_write_access returns TRUE,
      // the commit goes through. (This is for admin or sandbox exceptions.)
      $outcome = $function($operation, $operation_items);
      if ($outcome === TRUE) {
        return TRUE;
      }
      else { // if !TRUE, $outcome is required to be an array with error messages
        $error_messages = array_merge($error_messages, $outcome);
      }
    }

    // Let the operation fail if there's more than zero error messages.
    if (!empty($error_messages)) {
    $this->setAccessErrors($error_messages);
      return FALSE;
    }
    return TRUE;
  }

  /**
   * Get the user-visible version of a commit identifier a.k.a.
   * 'revision', as plaintext. By default, this function returns the
   * operation's revision if that property exists, or its vc_op_id
   * identifier as fallback.
   *
   * Version control backends can, however, choose to implement their
   * own version of this function, which for example makes it possible
   * to cut the SHA-1 hash in distributed version control systems down
   * to a readable length.
   *
   * @param $format
   *   Either 'full' for the original version, or 'short' for a more compact form.
   *   If the commit identifier doesn't need to be shortened, the results can
   *   be the same for both versions.
   */
  public function formatRevisionIdentifier($format = 'full') {
    if (empty($this->revision)) {
      return '#'. $this->vc_op_id;
    }
    return $this->repository->formatRevisionIdentifier($this->revision, $format);
  }

  /**
   * Retrieve the tag or branch that applied to that item during the
   * given operation. The result of this function will be used for the
   * selected label property of the item, which is necessary to preserve
   * the item state throughout navigational API functions.
   *
   * @param $item
   *   The item revision for which the label should be retrieved.
   *
   * @return
   *   NULL if the given item does not belong to any label or if the
   *   appropriate label cannot be retrieved. Otherwise a
   *   VersioncontrolLabel array is returned
   *
   *   In case the label array also contains the 'label_id' element
   *   (which happens when it's copied from the $operation->labels
   *   array) there will be a small performance improvement as the label
   *   doesn't need to be compared to and loaded from the database
   *   anymore.
   */
  public abstract function getSelectedLabel($item);

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
