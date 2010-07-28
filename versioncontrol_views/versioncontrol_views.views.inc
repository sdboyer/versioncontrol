<?php
// $Id$

/**
 * Implement hook_views_data().
 *
 * Describes schema for versioncontrol tables.
 *
 * @return array
 */

//TODO: 'action' handler

function versioncontrol_views_views_data() {
  $data = array();
  
  $data['versioncontrol_accounts']['table']['group'] = t("VersionControl Accounts");

  $data['versioncontrol_operations']['table']['group'] = t('VersionControl Operations');
  $data['versioncontrol_repositories']['table']['group'] = t("VersionControl Repository");

  $data['versioncontrol_operation_labels']['table']['group'] = t("VersionControl Labels");
  $data['versioncontrol_labels']['table']['group'] = t("VersionControl Labels");
  
  $data['versioncontrol_operation_items']['table']['group'] = t("VersionControl Item Revisions");
  $data['versioncontrol_item_revisions']['table']['group'] = t("VersionControl Item Revisions");
  $data['versioncontrol_source_items']['table']['group'] = t("VersionControl Source Items");
 

  $data['versioncontrol_operations']['table']['base'] = array(
    'field' => 'vc_op_id',
    'title' => t('VersonControl Operation'),
    'help'  => t('Repository operations are commit, tag and branch events'),
  );
  $data['versioncontrol_repositories']['table']['base'] = array(
    'field' => 'repo_id',
    'title' => t('VersonControl Repository'),
    'help'  => t('A repository object'),
  );
  
  $data['users']['table']['join'] = array(
    'versioncontrol_operations' => array(
      'left_field' => 'uid',
      'field'      => 'uid',
      'type'       => 'inner',
    ),
    'versioncontrol_accounts' => array(
      'left_field' => 'uid',
      'field' => 'uid',
      'type' => 'inner',
    ),
  );
  $data['versioncontrol_accounts']['table']['join'] = array(
    'user' => array(
      'left_field' => 'uid',
      'field' => 'uid',
    ),
    'versioncontrol_repository' => array(
      'left_field' => 'repo_id',
      'field' => 'repo_id',
    ),
  );
  
  $data['versioncontrol_operations']['table']['join'] = array(
    'users' => array(
      'left_field' => 'uid',
      'field'      => 'uid',
    ),

    'versioncontrol_repositories' => array(
      'left_field' => 'repo_id',
      'field' => 'repo_id',
    ),

    'versioncontrol_operation_labels' => array(
      'left_field' => 'vc_op_id',
      'field' => 'vc_op_id',
    ),
    'versioncontrol_labels' => array(
      'left_table' => 'versioncontrol_operation_labels',
      'left_field' => 'vc_op_id',
      'field' => 'vc_op_id',
    ),

    'versioncontrol_operation_items' => array(
      'left_field' => 'vc_op_id',
      'field' => 'vc_op_id',
    ),
    'versioncontrol_item_revisions' => array(
      'left_table' => 'versioncontrol_operation_items',
      'left_field' => 'vc_op_id',
      'field' => 'vc_op_id',
    ),
  );


  $data['versioncontrol_repositories']['table']['join'] = array(
    'versioncontrol_operations' => array(
      'left_field' => 'repo_id',
      'field' => 'repo_id',
    ),
    'versioncontrol_item_revisions' => array(
      'left_field' => 'repo_id',
      'field' => 'repo_id',
    ),
    'versioncontrol_accounts' => array(
      'left_field' => 'repo_id',
      'field' => 'repo_id',
    ),
  );


  $data['versioncontrol_operation_labels']['table']['join'] = array(
    'versioncontrol_labels' => array(
      'left_field' => 'label_id',
      'field' => 'label_id',
    ),
    'versioncontrol_operations' => array(
      'left_field' => 'vc_op_id',
      'field' => 'vc_op_id',
    ),
  );

  $data['versioncontrol_labels']['table']['join'] = array(
    'versioncontrol_operation_labels' => array(
      'left_field' => 'label_id',
      'field' => 'label_id',
    ),
    'versioncontrol_operations' => array(
      'left_table' => 'versioncontrol_operation_labels',
      'left_field' => 'label_id',
      'field' => 'label_id',
    ),
  );

  $data['versioncontrol_operation_items']['table']['join'] = array(
    'versioncontrol_item_revisions' => array(
      'left_field' => 'item_revision_id',
      'field' => 'item_revision_id',
    ),
    'versioncontrol_operations' => array(
      'left_field' => 'vc_op_id',
      'field' => 'vc_op_id',
    ),
  );

  $data['versioncontrol_item_revisions']['table']['join'] = array(
    'versioncontrol_operation_items' => array(
      'left_field' => 'item_revision_id',
      'field' => 'item_revision_id',
    ),
    'versioncontrol_operations' => array(
      'left_table' => 'versioncontrol_operation_items',
      'left_field' => 'item_revision_id',
      'field' => 'item_revision_id',
    ),

    'versioncontrol_repositories' => array(
      'left_field' => 'repo_id',
      'field' => 'repo_id',
    ),
    'versioncontrol_source_items' => array(
      'left_field' => 'item_revision_id',
      'field' => 'item_revision_id',
    ),
  );
  
  $data['versioncontrol_accounts'] += array(
    
  );
  
  $data['versioncontrol_operations'] += array(
    'vc_op_id'      => array(
      'title'    => t('Operation ID'),
      'help'     => t('The unique ID for a operation'),
      'field'    => array(
        'handler' => 'views_handler_field_numeric',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_numeric',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_numeric',
      ),
    ),
    'type'     => array(
      'title'    => t('Type'),
      'help'     => t('The type of operation. Tag, Commit or Branch'),
      'field'    => array(
        'handler' => 'versioncontrol_handler_field_operation_type',
         'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'versioncontrol_handler_filter_operation_type',
      ),
    ),
    'date'  => array(
      'title'    => t('Date'),
      'help'     => t('The date of the operation'),
      'field'    => array(
        'handler' => 'views_handler_field_date',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort_date',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_date',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_date',
      ),
    ),
    'uid' => array(
      'title' => t('User'),
      'help' => t('The user that did the operation'),
      'relationship' => array(
        'base'  => 'users',
        'base field' => 'uid',
        'handler' => 'views_handler_relationship',
        'label' => t('User'),
      ),
    ),
    'author' => array(
      'title'    => t('Author'),
      'help'     => t('The Operation Author'),
      'field'    => array(
        //TODO: Display handler to filter GIT usernames?
        'handler' => 'views_handler_field',
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_string',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_string',
      ),
    ),
    'committer' => array(
      'title'    => t('Commiter'),
      'help'     => t('The Operation Commiter'),
      'field'    => array(
        'handler' => 'views_handler_field',
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_string',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_string',
      ),
    ),
    'revision' => array(
      'title'    => t('Revision'),
      'help'     => t('The Operation Revision Identifier'),
      'field'    => array(
        'handler' => 'views_handler_field',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_string',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_string',
      ),
    ),
    'message' => array(
      'title'    => t('Message'),
      'help'     => t('The Operation Message'),
      'field'    => array(
        'handler' => 'views_handler_field',
      ),
    ),
  );


  $data['versioncontrol_repositories'] += array(
    'repo_id' => array(
      'title' => t("Repository ID"),
      'help' => t("The unique identifier of the repository"),
      'field' => array(
        'handler' => 'views_handler_field_numeric',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_numeric',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_numeric',
      ),
    ),
    'name' => array(
      'title' => t("Repository Name"),
      'help' => t("The name of the repository"),
      'field' => array(
        'handler' => 'views_handler_field',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_string',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_string',
      ),
    ),
    'vcs' => array(
      'title' => t("Backend Type"),
      'help' => t("The backend type of VCS that this repository uses"),
      'field' => array(
        'handler' => 'views_handler_field',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'versioncontrol_handler_filter_repo_backend',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_string',
      ),
    ),
    'root' => array(
      'title' => t("Root Path"),
      'help' => t("The root path to the repository"),
      'field' => array(
        'handler' => 'views_handler_field',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter',
      ),
    ),
  );

  $data['versioncontrol_operation_labels'] += array(
    'label_id' => array(
      'title' => t('Label List'),
      'help' => t('A list of labels this operation affected'),
      'field' => array(
        'handler' => 'versioncontrol_handler_field_operation_labels',
      ),
    ),

    //TODO: What is this again?
    'action' => array(
      'title' => t("Op Label Action"),
      'help' => t("The operations action on this label"),

      'field' => array(
        'handler' => 'views_handler_field_numeric',
      ),

    ),
  );
  $data['versioncontrol_labels'] += array(
    'label_id' => array(
      'title' => t("Label ID"),
      'help' => t("The unique label identifier"),
      'field' => array(
        'handler' => 'views_handler_field_numeric',
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_numeric',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_numeric',
      ),
    ),
    'name' => array(
      'title' => t("Label Name"),
      'help' => t("The name of this label"),
      'field' => array(
        'handler' => 'views_handler_field',
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'versioncontrol_handler_filter_operation_type_label',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_string',
      ),
    ),
    'type' => array(
      'title' => t('Label Type'),
      'help' => t('The type of the label. Tag or Branch'),
      //TODO: Handler 'field' and 'filter'
      'field' => array(
        'handler' => 'views_handler_field_numeric',
      ),
    ),
  );


  $data['versioncontrol_operation_items'] += array(
    'item_revision_id' => array(
      'title' => t('Item List'),
      'help' => t('A list of items that were affected by this operation.'),
      'field' => array(
        'handler' => 'versioncontrol_handler_field_operation_items',
      ),
    ),

    //TODO: Confirm what this is.
    'type' => array(
      'title' => t("Op Item Type"),
      'help' => t("The type of items' operation"),
      'field' => array(
        'handler' => 'views_handler_field_numeric',
      ),
    ),
  );

  $data['versioncontrol_item_revisions'] += array(
    'path' => array(
      'title' => t("File path"),
      'help' => t("The file path within the repository"),
      'field' => array(
        'handler' => 'views_handler_field',
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'filter' => array(
        'handler' => 'views_handler_filter_string',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_string',
      ),
    ),
    'revision' => array(
      'title' => t("Revision ID"),
      'help' => t("The revision identifier for this item"),
      'field' => array(
        'handler' => 'views_handler_field',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
      'argument' => array(
        'handler' => 'views_handler_argument_string',
      ),
    ),
    'type' => array(
      'title' => t("Op Item Type"),
      'help' => t("The type of item's operation"),
      'field' => array(
        'handler' => 'views_handler_field_numeric',
      ),
    ),
  );

  $data['versioncontrol_source_items'] += array(
    'item_revision_id' => array(

    ),
    'source_item_revision_id' => array(

    ),
    'action' => array(
      'title' => t('Action'),
      'help' => t('The action performed on this item'),
      'field' => array(
        //TODO: Action field
        'handler' => 'views_handler_field_generic',
      ),
    ),
    'line_changes_recorded' => array(
      
    ),
    'line_changes_added' => array(
      'title' => t("Lines Added"),
      'help' => t("The number of lines added to this item"),
      'field' => array(
        'handler' => 'views_handler_field_numeric',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ),
    ),
    'line_changes_removed' => array(
      'title' => t("Lines Removed"),
      'help' => t("The number of lines removed from this item"),
      'field' => array(
        'handler' => 'views_handler_field_numeric',
        'click sortable' => TRUE,
      ),
      'sort' => array(
        'handler' => 'views_handler_sort',
      ), 
    ),
    
  );
  
  return $data;
}



/**
 * Implement hook_views_handler().
 *
 * @return array
 */
function versioncontrol_views_views_handlers() {
  $ret = array(
    'info' => array(
      'path'       => drupal_get_path('module', 'versioncontrol_views') . '/handlers',
    ),
    'handlers' => array(
      // field handlers
      'versioncontrol_handler_field_operation_type' => array(
        'parent' => 'views_handler_field',
      ),
      'versioncontrol_handler_field_operation_items' => array(
        'parent' => 'views_handler_field_prerender_list',
      ),
      'versioncontrol_handler_field_operation_labels' => array(
        'parent' => 'views_handler_field_prerender_list',
      ),
      'versioncontrol_handler_filter_operation_type' => array(
        'parent' => 'views_handler_filter_in_operator',
      ),
      'versioncontrol_handler_filter_operation_label' => array(
        'parent' => 'views_handler_filter_in_operator',
      ),
      'versioncontrol_handler_filter_repo_backend' => array(
        'parent' => 'views_handler_filter_in_operator',
      ),
      'versioncontrol_handler_filter_operation_type_label' => array(
        'parent' => 'views_handler_filter_string',
      ),
    ),
  );

  return $ret;
}


/**
 * Implementation of hook_views_plugins()
 *
 * @return array
 */
function versioncontrol_views_views_plugins() {
  return array(
    'module' => 'versioncontrol_views',
    'row' => array(
      'versioncontrol_operations' => array(
        'title' => t('Operation'),
        'help' => t('Display a rendered operation'),
        'handler' => 'versioncontrol_plugin_row_operation',
        'path' => drupal_get_path('module', 'versioncontrol_views') . '/handlers',
        'base' => array(
          'versioncontrol_operations',
        ),
        'type' => 'normal',
        'uses fields' => FALSE,
      ),
    ),
    'style' => array(
      'versioncontrol_operations' => array(
        'title' => t('Versioncontrol Operations'),
        'help' => t('Displays a grouped list of operations'),
        'handler' => 'versioncontrol_plugin_style_operations',
        'path' => drupal_get_path('module', 'versioncontrol_views') . '/handlers',
        'base' => array(
          'versioncontrol_operations',
        ),
        'type' => 'normal',
        'uses grouping' => TRUE,
        'uses row plugin' => TRUE,
        'theme' => 'versioncontrol_views_style_operations',
      ),
    ),
  );
}

