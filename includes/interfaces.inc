<?php
// $Id$

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
