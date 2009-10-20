<?php
// $Id$
/**
 * @file
 * Item class
 */

require_once 'VersioncontrolRepository.php';

/**
 * @name VCS item types.
 */
//@{
define('VERSIONCONTROL_ITEM_FILE',              1);
define('VERSIONCONTROL_ITEM_DIRECTORY',         2);
/**
 * @name VCS "Deleted" item types.
 * Only used for items that don't exist in the repository (anymore), at least
 * not in the given revision. That is mostly the case with items that
 * were deleted by a commit and are returned as result by
 * VersioncontrolOperation::getItems(). A "deleted file" can also be
 * returned by directory listings for CVS, representing "dead files".
 */
//@{
define('VERSIONCONTROL_ITEM_FILE_DELETED',      3);
define('VERSIONCONTROL_ITEM_DIRECTORY_DELETED', 4);
//@}
//@}

/**
 * Represent an Item (a.k.a. item revisions)
 *
 * Files or directories inside a specific repository, including information
 * about the path, type ("file" or "directory") and (file-level)
 * revision, if applicable. Most item revisions, but probably not all of
 * them, are recorded in the database.
 */
abstract class VersioncontrolItem implements ArrayAccess {
  /**
   * DB identifier.
   *
   * @var    int
   */
  public $item_revision_id;

  /**
   * The path of the item.
   *
   * @var    string
   */
  public $path;

  /**
   * Deleted status.
   *
   * @var    boolean
   */
  public $deleted;

  /**
   * A specific revision for the requested item, in the same VCS-specific
   * format as $item->revision. A repository/path/revision combination is
   * always unique, so no additional information is needed.
   *
   * @var    string
   */
  public $revision;

  /**
   * FIXME: ?
   *
   * @var    array
   */
  public $source_items = array();

  /**
   * For a single item (file or directory) in a commit, or for branches
   * and tags. Either
   * VERSIONCONTROL_ACTION_{ADDED,MODIFIED,MOVED,COPIED,MERGED,DELETED,
   * REPLACED,OTHER}
   *
   * @var    array
   */
  public $action;

  /**
   * Let count added/removed lines if possible.
   *
   * It has the following elements:
   *
   * - 'added': Number of lines added to the repository in this
   * VersioncontrolItem.
   * - 'removed': Number of lines removed to the repository in this
   * VersioncontrolItem.
   *
   * @var    array
   */
  public $line_changes = array();

  /**
   * The repository where the operation was done.
   *
   * @var    VersioncontrolRepository
   */
  public $repository;

  //TODO subclass per type?
  public $selected_label;
  public $commit_operation;

  /**
   * FIXME: ?
   */
  private static $successor_action_priority = array(
    VERSIONCONTROL_ACTION_MOVED => 10,
    VERSIONCONTROL_ACTION_MODIFIED => 10,
    VERSIONCONTROL_ACTION_COPIED => 8,
    VERSIONCONTROL_ACTION_MERGED => 9,
    VERSIONCONTROL_ACTION_OTHER => 1,
    VERSIONCONTROL_ACTION_DELETED => 1,
    VERSIONCONTROL_ACTION_ADDED => 0, // does not happen, guard nonetheless
    VERSIONCONTROL_ACTION_REPLACED => 0, // does not happen, guard nonetheless
  );

  /**
   * Constructor.
   */
  public function __construct($type, $path, $revision, $action, $repository, $deleted = NULL, $item_revision_id = NULL) {
    $this->type = $type;
    $this->path = $path;
    $this->revision = $revision;
    $this->action = $action;
    $this->repository = $repository;
    $this->deleted = $deleted;
    $this->item_revision_id = $item_revision_id;
  }

  /**
   * Return TRUE if the given item is an existing or an already deleted
   * file, or FALSE if it's not.
   */
  public function isFile() {
    if ($this->type == VERSIONCONTROL_ITEM_FILE
      || $this->type == VERSIONCONTROL_ITEM_FILE_DELETED) {
        return TRUE;
      }
    return FALSE;
  }

  /**
   * Return TRUE if the given item is an existing or an already deleted
   * directory, or FALSE if it's not.
   */
  public function isDirectory($item) {
    if ($this->type == VERSIONCONTROL_ITEM_DIRECTORY
      || $this->type == VERSIONCONTROL_ITEM_DIRECTORY_DELETED) {
        return TRUE;
      }
    return FALSE;
  }

  /**
   * Return TRUE if the given item is marked as deleted, or FALSE if it exists.
   */
  public function isDeleted($item) {
    if ($this->type == VERSIONCONTROL_ITEM_FILE_DELETED
      || $this->type == VERSIONCONTROL_ITEM_DIRECTORY_DELETED) {
        return TRUE;
      }
    return FALSE;
  }

  /**
   * Retrieve the revisions where the given item has been changed,
   * in reverse chronological order.
   *
   * Only one direct source or successor of each item will be retrieved,
   * which means that you won't get parallel history logs with a single
   * function call. In order to retrieve the log for this item in a
   * different branch, you need to switch the selected label of the item
   * by retrieving a different version of it with a call of
   * Item::getParallelItems() (if the backend supports this function).
   *
   * TODO: params doc
   *
   * @return
   *   An array containing a list of item arrays, each one specifying a
   *   revision of the same item that was given as argument. The array is
   *   sorted in reverse chronological order, so the newest revision
   *   comes first. Each element has its (file-level) item revision as
   *   key, and a standard item object (as the ones retrieved by
   *   VersioncontrolOperation::getItems()) as value. All items except
   *   for the oldest one will also have the 'action' and 'source_items'
   *   properties filled in, the oldest item might or might not have
   *   them. (If they exist for the oldest item, 'action' will be
   *   VERSIONCONTROL_ACTION_ADDED and 'source_items' an empty array.)
   *
   *   NULL is returned if the given item is not under version control,
   *   or was not under version control at the time of the given
   *   revision, or if no history could be retrieved for any other
   *   reason.
   */
  public function getHistory($successor_item_limit = NULL, $source_item_limit = NULL) {
    // Items without revision have no history, don't even try to fetch it.
    if (empty($this->revision)) {
      return NULL;
    }
    // If we don't yet know the item_revision_id (required for db
    // queries), try to retrieve it. If we don't find it, we can't go on
    // with this function.
    if (!$this->fetchItemRevisionId()) {
      return NULL;
    }

    // Make sure we don't run into infinite loops when passed bad
    // arguments.
    if (is_numeric($successor_item_limit) && $successor_item_limit < 0) {
      $successor_item_limit = 0;
    }
    if (is_numeric($source_item_limit) && $source_item_limit < 0) {
      $source_item_limit = 0;
    }

    // Naive implementation - can probably be improved by sticking to
    // the samerepo_id/path until an action other than "modified" or
    // "other" appears. (With the drawback that code will probably need
    // to be duplicated among this function and
    // versioncontrol_fetch_{source,successor}_items().

    // Find (recursively) all successor items within the successor item
    // limit.
    $history_successor_items = array();
    $source_item = $this;

    while ((!isset($successor_item_limit) || ($successor_item_limit > 0))) {
      $source_items = array($source_item->path => $source_item);
      versioncontrol_fetch_successor_items($this->repository, $source_items);
      $source_item = $source_items[$source_item->path];

      // If there are no successor items, we are obviously at the end of
      // the log.
      if (empty($source_item->successor_items)) {
        break;
      }
      // There might be multiple successor items - in most cases, the
      // first one is the only one so that's ok except for "merged"
      // actions.
      $successor_item = NULL;
      $highest_priority_so_far = 0;
      foreach ($source_item->successor_items as $path => $succ_item) {
        if (!isset($successor_item)
          || self::$successor_action_priority[$succ_item->action] > $highest_priority_so_far) {
            $successor_item = $succ_item;
            $highest_priority_so_far = self::$successor_action_priority[$succ_item->action];
          }
      }
      $history_successor_items[$successor_item->revision] = $successor_item;
      $source_item = $successor_item;

      // Decrement the counter until the item limit is reached.
      if (isset($successor_item_limit)) {
        --$successor_item_limit;
      }
    }
    // We want the newest revisions first, so reverse the successor array.
    $history_successor_items = array_reverse($history_successor_items, TRUE);

    // Find (recursively) all source items within the source item limit.
    $history_source_items = array();
    $successor_item = $this;

    while (!isset($source_item_limit) || ($source_item_limit > 0)) {
      $successor_items = array($successor_item->path => $successor_item);
      versioncontrol_fetch_source_items($repository, $successor_items);
      $successor_item = $successor_items[$successor_item->path];

      // If there are no source items, we are obviously at the end of the log.
      if (empty($successor_item->source_items)) {
        break;
      }
      // There might be multiple source items - in most cases, the first one is
      // the only one so that's ok except for "merged" actions.
      $source_item = NULL;
      if ($successor_item->action == VERSIONCONTROL_ACTION_MERGED) {
        if (isset($successor_item->source_items[$successor_item->path])) {
          $source_item = $successor_item->source_items[$successor_item->path];
        }
      }
      if (!isset($source_item)) {
        $source_item = reset($successor_item->source_items); // first item
      }
      $history_source_items[$source_item->revision] = $source_item;
      $successor_item = $source_item;

      // Decrement the counter until the item limit is reached.
      if (isset($source_item_limit)) {
        --$source_item_limit;
      }
    }

    return $history_successor_items + array($this->revision => $this) + $history_source_items;
  }

  /**
   * Make sure that the 'item_revision_id' database identifier is among
   * an item's properties, and if it's not then try to add it.
   *
   * @return
   *   TRUE if the 'item_revision_id' exists after calling this
   *   function, FALSE if not.
   */
  public function fetchItemRevisionId() {
    if (!empty($this->item_revision_id)) {
      return TRUE;
    }
    $id = db_result(db_query(
      "SELECT item_revision_id FROM {versioncontrol_item_revisions}
    WHERE repo_id = %d AND path = '%s' AND revision = '%s'",
    $this->repository->repo_id, $this->path, $this->revision
  ));
    if (empty($id)) {
      return FALSE;
    }
    $this->item_revision_id = $id;
    return TRUE;
  }

  /**
   * Retrieve an item's selected label.
   *
   * When first retrieving an item, the selected label is initialized
   * with a sensible value - for example,
   * VersioncontrolOperation::getItems() assigns the affected branch or
   * tag of that operation to all the items. (This is especially
   * important for version control systems like Subversion where there is
   * a need to specify the label per item and not per operation, as a
   * single commit can affect multiple branches or tags at once.)
   *
   * The selected label is also meant to help with branch/tag-based
   * navigation, so item navigation functions will try to preserve it as
   * good as possible, as far as it's accurate.
   *
   * @return
   *   In case no branch or tag applies to that item or could not be
   *   retrieved for whatever reasons, the selected label can also be
   *   NULL. Otherwise, it's a VersioncontrolLabel object(tag or branch)
   */
  public function getSelectedLabel() {
    // If the label is already retrieved, we can return it just that way.
    if (isset($this->selected_label->label)) {
      return ($this->selected_label->label === FALSE)
        ? NULL : $this->selected_label->label;
    }
    if (!isset($this->selected_label->get_from)) {
      $this->selected_label->label = FALSE;
      return NULL;
    }

    // Otherwise, determine how we might be able to retrieve the selected
    // label.
    switch ($this->selected_label->get_from) {
    case 'operation':
      $selected_label = $this->selected_label->operation->getSelectedLabel($this);
      break;
    case 'other_item':
      $selected_label = $this->getSelectedLabelFromItem($this->selected_label->other_item, $this->selected_label->other_item_tags);
      unset($this->selected_label->other_item_tags);
      break;
    }

    if (isset($selected_label)) {
      // Just to make sure that we only pass applicable info:
      // 'action' might make sense in an operation, but not in an item
      // object.
      if (isset($selected_label->action)) {
        //FIXME we are returning a label here, not an item; so, is it ok to have an action on label?
        //  unset($selected_label->action);
      }
      $selected_label->ensure();
      $this->selected_label->label = $selected_label;
    }
    else {
      $this->selected_label->label = FALSE;
    }

    // Now that we've got the real label, we can get rid of the retrieval
    // recipe.
    if (isset($this->selected_label->{$this->selected_label->get_from})) {
      unset($this->selected_label->{$this->selected_label->get_from});
    }
    unset($this->selected_label->get_from);

    return $this->selected_label->label;
  }

  /**
   * Check if the @p $path_regexp applies to the path of the given @p
   * $item.
   *
   * This function works just like preg_match(), with the single
   * difference that it also accepts a trailing slash for item paths if
   * the item is a directory.
   *
   * @return
   *   The number of times @p $path_regexp matches. That will be either 0
   *   times (no match) or 1 time because preg_match() (which is what
   *   this function uses internally) will stop searching after the first
   *   match.
   *   FALSE will be returned if an error occurred.
   */
  public function pregMatch($path_regexp) {
    $path = $this->path;

    if ($this->isDirectory() && $path != '/') {
      $path .= '/';
    }
    return preg_match($path_regexp, $path);
  }

  /**
   * Print out a "Bad item received from VCS backend" warning to
   * watchdog.
   */
  protected function badItemWarning($message) {
    watchdog('special', "<p>Bad item received from VCS backend: !message</p>
      <pre>Item object: !item\n</pre>", array(
        '!message' => $message,
        '!item' => print_r($this, TRUE),
      ), WATCHDOG_ERROR
    );
  }

  /**
   * Retrieve the parent (directory) item of a given item.
   *
   * @param $parent_path
   *   NULL if the direct parent of the given item should be retrieved,
   *   or a parent path that is further up the directory tree.
   *
   * @return
   *   The parent directory item at the same revision as the given item.
   *   If $parent_path is not set and the item is already the topmost one
   *   in the repository, the item is returned as is. It also stays the
   *   same if $parent_path is given and the same as the path of the
   *   given item. If the given directory path does not correspond to a
   *   parent item, NULL is returned.
   */
  public function getParentItem($parent_path = NULL) {
    if (!isset($parent_path)) {
      $path = dirname($this->path);
    }
    elseif ($this->path == $parent_path) {
      return $this;
    }
    elseif ($parent_path == '/' || strpos($this->path .'/', $parent_path .'/') !== FALSE) {
      $path = $parent_path;
    }
    else {
      return NULL;
    }

    $revision = '';
    if (in_array(VERSIONCONTROL_CAPABILITY_DIRECTORY_REVISIONS, $this->repository->backend->capabilities)) {
      $revision = $this->revision;
    }

    $parent_item = new $this->repository->backend->classes['item'](VERSIONCONTROL_ITEM_DIRECTORY,
      $path, $revision, NULL, $this->repository);

    $parent_item->selected_label = new stdClass();
    $parent_item->selected_label->get_from = 'other_item';
    $parent_item->selected_label->other_item = &$this;
    $parent_item->selected_label->other_item_tags = array('same_revision');

    return $parent_item;
  }

  /**
   * Given an item in a repository, retrieve related versions of that
   * item on all different branches and/or tags where the item exists.
   *
   * VersioncontrolItemParallelItems interface is optional for VCS
   * backends to implement, be sure to check the return value to NULL.
   *
   * @param $label_type_filter
   *   If unset, siblings will be retrieved both on branches and tags.
   *   If set to VERSIONCONTROL_LABEL_BRANCH or VERSIONCONTROL_LABEL_TAG,
   *   results are limited to just that label type.
   *
   * @return
   *   An item array of parallel items on all branches and tags, possibly
   *   including the original item itself (if appropriate for the given
   *   @p $label_type_filter). Array keys do not convey any specific
   *   meaning, item values are VersioncontrolItem objects.
   *
   *   Branch and tag names are implicitely stored and can be retrieved
   *   by calling Item::getSelectedLabel() on each item in the result
   *   array.
   *
   *   NULL is returned if the given item is not inside the repository,
   *   or has not been inside the repository at the specified revision.
   *   An empty array is returned if the item is valid, but no parallel
   *   sibling items can be found for the given @p $label_type.
   */
  public final function getParallelItems($label_type_filter = NULL) {
    if ($this instanceof VersioncontrolItemParallelItems) {
      $results = $this->_getParallelItems($label_type_filter);
    }
    else {
      return NULL;
    }
    if (is_null($results)) {
      return NULL;
    }
    $items = array();

    foreach ($results as $key => $result) {
      $items[$key] = $result['item'];
      $items[$key]['selected_label'] = new stdClass();
      $items[$key]['selected_label']->label = is_null($result['selected_label'])
        ? NULL
        : $result['selected_label'];
    }
    return $items;
  }

  /**
   * Retrieve the set of files and directories that exist at a specified
   * revision inside the given directory in the repository.
   *
   * This function is optional for VCS backends to implement, be sure to
   * check the return value to NULL.
   *
   * @param $recursive
   *   If FALSE, only the direct children of $path will be retrieved.
   *   If TRUE, you'll get every single descendant of $path.
   *
   * @return
   *   A structured item array of items that have been inside the
   *   directory in its given state, including the directory item itself.
   *   Array keys are the current/new paths. The corresponding item
   *   values are again structured arrays and consist of elements with
   *   the following keys:
   *
   *   - 'type': Specifies the item type, which is either
   *   VERSIONCONTROL_ITEM_FILE or VERSIONCONTROL_ITEM_DIRECTORY.
   *   - 'path': The path of the item at the specific revision.
   *   - 'revision': The (file-level) revision when the item was last
   *   changed. If there is no such revision (which may be the case for
   *   directory items) then the 'revision' element is an empty string.
   *
   *   NULL is returned if the given item is not inside the repository,
   *   or if it is not a directory item at all.
   *
   *   A real-life example of such a result array can be found in the
   *   FakeVCS example module.
   */
  public function getDirectoryContents($recursive = FALSE) {
    if (!$this->isDirectory() || !$this instanceof VersioncontrolItemDirectoryContents) {
      return NULL;
    }
    $contents = $this->_getDirectoryContents($recursive);
    if (!isset($contents)) {
      return NULL;
    }
    $items = array();

    foreach ($contents as $path => $content) {
      $items[$path] = $content['item'];
      $items[$path]['selected_label'] = new stdClass();
      $items[$path]['selected_label']->label = is_null($content['selected_label'])
        ? NULL
        : $content['selected_label'];
    }
    return $items;
  }

  /**
   * Retrieve a copy of the contents of a given file item in the
   * repository.
   *
   * (You won't get the original because repositories can often be
   * remote.)
   *
   * The caller should make sure to delete the file when it's not needed
   * anymore. That requirement might change in the future though.
   *
   * This function is optional for VCS backends to implement, be sure to
   * check the return to NULL.
   *
   * @return
   *   The local path of the created copy, if successful.
   *   NULL is returned if the given item is not under version control,
   *   or was not under version control at the time of the given
   *   revision.
   */
  public function exportFile() {
    if (!$this->isFile()) {
      return NULL;
    }
    $filename = basename($file_item['path']);
    $destination = file_directory_temp() .'/versioncontrol-'. mt_rand() .'-'. $filename;
    if ($this instanceof VersioncontrolItemExportFile) {
      $success = $this->_exportFile($destination);
    }
    else {
      return NULL;
    }
    if ($success) {
      return $destination;
    }
    @unlink($destination);
    return NULL;
  }

  /**
   * Retrieve a copy of the given directory item in the repository.
   *
   * (You won't get the original because repositories can often be
   * remote.)
   *
   * The caller should make sure to delete the directory when it's not
   * needed anymore.
   *
   * This function is optional for VCS backends to implement, be sure to
   * check return to NULL.
   *
   * @param $destination_dirpath
   *   The path of the directory that will receive the contents of the
   *   exported repository item. If that directory already exists, it
   *   will be replaced. If that directory doesn't yet exist, it will be
   *   created by the backend. (This directory will directly correspond
   *   to the @p $directory_item - there are no artificial
   *   subdirectories, even if the @p $destination_dirpath has a
   *   different basename than the original path of the @p
   *   $directory_item.)
   *
   * @return
   *   TRUE if successful, or FALSE if not.
   *   FALSE can be returned if the given item is not under version
   *   control, or was not under version control at the time of the given
   *   revision, or simply cannot be exported to the destination
   *   directory for any reason.
   */
  public function exportDirectory($destination_dirpath) {
    if (!$item->isDirectory()) {
      return FALSE;
    }
    // Unless file.inc provides a nice function for recursively deleting
    // directories, let's just go for the straightforward portable method.
    $rm = (drupal_strtoupper(drupal_substr(PHP_OS, 0, 3)) == 'WIN') ? 'rd /s' : 'rm -rf';
    exec("$rm $destination_dirpath");

    if ($this instanceof VersioncontrolItemExportDirectory) {
      $success = $this->_exportDirectory($destination_dirpath);
    }
    else {
      return FALSE;
    }
    if (!$success) {
      exec("$rm $destination_dirpath");
      return FALSE;
    }
    return TRUE;
  }

  /**
   * Retrieve an array where each element represents a single line of the
   * given file in the specified commit, annotated with the committer who
   * last modified that line. Note that annotations are generally a quite
   * slow operation, so expect this function to take a bit more time as
   * well.
   *
   * This function is optional for VCS backends to implement, be sure to
   * check the return to NULL.
   *
   * @return
   *   A structured array that consists of one element per line, with
   *   line numbers as keys (starting from 1) and a structured array as
   *   values, where each of them consists of elements with the following
   *   keys:
   *
   *   - 'username': The system specific VCS username of the last
   *   committer.
   *   - 'line': The contents of the line, without linebreak characters.
   *
   *   NULL is returned if the given item is not under version control,
   *   or was not under version control at the time of the given
   *   revision, or if it is not a file item at all, or if it is marked
   *   as binary file.
   *
   *   A real-life example of such a result array can be found in the
   *   FakeVCS example module.
   */
  public function getFileAnnotation() {
    if (!$this->isFile() || $this instanceof VersioncontrolItemGetFileAnnotation) {
      return NULL;
    }
    return $this->_getFileAnnotation();
  }

  /**
   * Check and if necessary correct item arrays so that item type and the
   * number of source items correspond to specified actions.
   */
  public function sanitize() {
    if (isset($this->action)) {
      // Make sure the number of source items corresponds with the action.
      switch ($this->action) {
        // No source items for "added" actions.
      case VERSIONCONTROL_ACTION_ADDED:
        if (count($this->source_items) > 0) {
          $this->badItemWarning('At least one source item exists although the "added" action was set (which mandates an empty \'source_items\' array.');
          $this->source_items = array(reset($this->source_items)); // first item
          $this->source_items = array();
        }
        break;
        // Exactly one source item for actions other than "added", "merged" or "other".
      case VERSIONCONTROL_ACTION_MODIFIED:
      case VERSIONCONTROL_ACTION_MOVED:
      case VERSIONCONTROL_ACTION_COPIED:
      case VERSIONCONTROL_ACTION_DELETED:
        if (count($this->source_items) > 1) {
          $this->badItemWarning('More than one source item exists although a "modified", "moved", "copied" or "deleted" action was set (which allows only one of those).');
          $item->source_items = array(reset($item->source_items)); // first item
        }
        // fall through
      case VERSIONCONTROL_ACTION_MERGED:
        if (empty($this->source_items)) {
          $this->badItemWarning('No source item exists although a "modified", "moved", "copied", "merged" or "deleted" action was set (which requires at least or exactly one of those).');
        }
        break;
      default:
        break;
      }
      // For a "delete" action, make sure the item type is also a "deleted" one.
      // That's quite a minor error, so don't complain but rather fix it quietly.
      if ($this->action == VERSIONCONTROL_ACTION_DELETED) {
        if ($this->type == VERSIONCONTROL_ITEM_FILE) {
          $this->type = VERSIONCONTROL_ITEM_FILE_DELETED;
        }
        elseif ($this->type == VERSIONCONTROL_ITEM_DIRECTORY) {
          $this->type = VERSIONCONTROL_ITEM_DIRECTORY_DELETED;
        }
      }
    }
  }

  /**
   * Insert an item entry into the {versioncontrol_source_items} table.
   *
   * Both target and source items are expected to have an
   * 'item_revision_id' property already. For "added" actions, it's also
   * possible to pass 0 as the @p $source_item parameter instead of a
   * full item array.
   */
  public function insertSourceRevision($source_item, $action) {
    if ($action == VERSIONCONTROL_ACTION_ADDED && $source_item === 0) {
      $source_item = new stdClass();
      $source_item->item_revision_id = 0;
    }
    // Before inserting that item entry, make sure it doesn't exist already.
    db_query("DELETE FROM {versioncontrol_source_items}
    WHERE item_revision_id = %d AND source_item_revision_id = %d",
    $this->item_revision_id, $source_item->item_revision_id);

    $line_changes = !empty($this->line_changes);
    db_query("INSERT INTO {versioncontrol_source_items}
    (item_revision_id, source_item_revision_id, action,
    line_changes_recorded, line_changes_added, line_changes_removed)
    VALUES (%d, %d, %d, %d, %d, %d)",
      $this->item_revision_id, $source_item->item_revision_id,
      $action, ($line_changes ? 1 : 0),
      ($line_changes ? $this->line_changes['added'] : 0),
      ($line_changes ? $this->line_changes['removed'] : 0));
  }

  /**
   * Insert an item entry into the {versioncontrol_item_revisions} table,
   * or retrieve the same one that's already there on the object.
   */
  public function ensure() {
    $result = db_query(
      "SELECT item_revision_id, type
       FROM {versioncontrol_item_revisions}
       WHERE repo_id = %d AND path = '%s' AND revision = '%s'",
       $this->repository->repo_id, $this->path, $this->revision
    );
    while ($item_revision = db_fetch_object($result)) {
      // Replace / fill in properties that were not in the WHERE condition.
      $this->item_revision_id = $item_revision->item_revision_id;

      if ($this->type == $item_revision->type) {
        return; // no changes needed - otherwise, replace the existing item.
      }
    }
    // The item doesn't yet exist in the database, so create it.
    $this->insert();
  }

  /**
   * Insert an item revision entry into the {versioncontrol_items_revisions}
   * table.
   */
  public function insert() {
    $this->repo_id = $this->repository->repo_id; // for drupal_write_record() only

    if (isset($this->item_revision_id)) {
      // The item already exists in the database, update the record.
      drupal_write_record('versioncontrol_item_revisions', $this, 'item_revision_id');
    }
    else {
      // The label does not yet exist, create it.
      // drupal_write_record() also adds the 'item_revision_id' to the $item array.
      drupal_write_record('versioncontrol_item_revisions', $this);
    }
    unset($this->repo_id);
  }

  /**
   * Get the user-visible version of an item's revision identifier, as
   * plaintext.
   * By default, this function simply returns $item['revision'].
   *
   * Version control backends can, however, choose to implement their own
   * version of this function, which for example makes it possible to cut
   * the SHA-1 hash in distributed version control systems down to a
   * readable length.
   *
   * @param $format
   *   Either 'full' for the original version, or 'short' for a more
   *   compact form.
   *   If the revision identifier doesn't need to be shortened, the
   *   results can be the same for both versions.
   */
  public function formatRevisionIdentifier($format = 'full') {
    return $this->repository->formatRevisionIdentifier($this->revision);
  }

  /**
   * Retrieve a valid label (tag or branch) for a new @p $target_item
   * that is (hopefully) similar or related to that of the given @p
   * $other_item which already has a selected label assigned. If the
   * backend cannot find a related label, return any valid label. The
   * result of this function will be used for the selected label property
   * of each item, which is necessary to preserve the item state
   * throughout navigational API functions.
   *
   * @param $other_item
   *   The item revision that the selected label should be derived from.
   *   For example, if @p $other_item in a CVS repository is at revision
   *   '1.5.2.1' which is on the 'DRUPAL-6--1' branch, and the @p
   *   $target_item is at revision '1.5' (its predecessor) which is
   *   present on both the 'DRUPAL-6--1' and 'HEAD' branches, then this
   *   function should return a label array for the 'DRUPAL-6--1' branch.
   * @param $other_item_tags
   *   An array with a simple list of strings that describe properties of
   *   the @p $other_item, in relation to the @p $target_item. You can
   *   use those in order to make assumptions so that the selected label
   *   can be retrieved more accurately or with better performance.
   *   Version Control API passes a list that may contain zero or more of
   *   the following tags:
   *
   *   - 'source_item': The @p $other_item is a predecessor of the @p
   *   $target_item - same entity, but in an earlier revision and
   *   potentially with a different path, too (only if the backend
   *   supports item moves).
   *   - 'successor_item': The @p $other_item is a successor of the @p
   *   $target_item - same entity, but in a later revision and
   *   potentially with a different path, too (only if the backend
   *   supports item moves).
   *   - 'same_revision': The @p $other_item is at the same (global)
   *   revision as the @p $target_item. Specifically meant for backends
   *   whose version control systems don't support atomic commits.
   *
   * @return
   *   NULL if the given item does not belong to any label or if an
   *   appropriate label cannot be retrieved. Otherwise a
   *   VersioncontrolLabel object is returned.
   *   In case the label array also contains the 'label_id' element
   *   (which happens when it's copied from the $operation->labels
   *   array) there will be a small performance improvement as the label
   *   doesn't need to be compared to and loaded from the database
   *   anymore.
   */
  public abstract function getSelectedLabelFromItem(&$other_item, $other_item_tags = array());

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
