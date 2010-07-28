<?php
// $Id$

/**
 * Programatically defines views for automatic loading on module installation
 *
 * Code created by using the views_export module
 */
function versioncontrol_views_views_default_views() {
  $view = new view;
  $view->name = 'versioncontrol_commitlog';
  $view->description = '';
  $view->tag = '';
  $view->view_php = '';
  $view->base_table = 'versioncontrol_operations';
  $view->is_cacheable = FALSE;
  $view->api_version = 2;
  $view->disabled = FALSE; /* Edit this to true to make a default view disabled initially */
  $handler = $view->new_display('default', 'Defaults', 'default');
  $handler->override_option('sorts', array(
    'date' => array(
      'order' => 'DESC',
      'granularity' => 'second',
      'id' => 'date',
      'table' => 'versioncontrol_operations',
      'field' => 'date',
      'override' => array(
        'button' => 'Override',
      ),
      'relationship' => 'none',
    ),
  ));
  $handler->override_option('filters', array(
    'author' => array(
      'operator' => '=',
      'value' => '',
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'author_op',
        'identifier' => 'author',
        'label' => 'Author',
        'optional' => 1,
        'remember' => 0,
      ),
      'case' => 1,
      'id' => 'author',
      'table' => 'versioncontrol_operations',
      'field' => 'author',
      'relationship' => 'none',
    ),
    'committer' => array(
      'operator' => '=',
      'value' => '',
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'committer_op',
        'identifier' => 'committer',
        'label' => 'Commiter',
        'optional' => 1,
        'remember' => 0,
      ),
      'case' => 1,
      'id' => 'committer',
      'table' => 'versioncontrol_operations',
      'field' => 'committer',
      'relationship' => 'none',
    ),
    'vc_op_id' => array(
      'operator' => '=',
      'value' => array(
        'value' => '',
        'min' => '',
        'max' => '',
      ),
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'vc_op_id_op',
        'identifier' => 'op_id',
        'label' => 'Operation ID',
        'optional' => 1,
        'remember' => 0,
      ),
      'id' => 'vc_op_id',
      'table' => 'versioncontrol_operations',
      'field' => 'vc_op_id',
      'relationship' => 'none',
    ),
    'revision' => array(
      'operator' => '=',
      'value' => '',
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'revision_op',
        'identifier' => 'revision',
        'label' => 'Revision',
        'optional' => 1,
        'remember' => 0,
      ),
      'case' => 1,
      'id' => 'revision',
      'table' => 'versioncontrol_operations',
      'field' => 'revision',
      'relationship' => 'none',
    ),
    'type' => array(
      'operator' => 'in',
      'value' => array(),
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'type_op',
        'identifier' => 'op_type',
        'label' => 'Operation Type Id',
        'optional' => 1,
        'single' => 1,
        'remember' => 0,
        'reduce' => 0,
      ),
      'id' => 'type',
      'table' => 'versioncontrol_operations',
      'field' => 'type',
      'relationship' => 'none',
    ),
    'vcs' => array(
      'operator' => 'in',
      'value' => array(),
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'vcs_op',
        'identifier' => 'vcs',
        'label' => 'Backend Type',
        'optional' => 1,
        'single' => 1,
        'remember' => 0,
        'reduce' => 0,
      ),
      'id' => 'vcs',
      'table' => 'versioncontrol_repositories',
      'field' => 'vcs',
      'relationship' => 'none',
    ),
    'repo_id' => array(
      'operator' => '=',
      'value' => array(
        'value' => '',
        'min' => '',
        'max' => '',
      ),
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'repo_id_op',
        'identifier' => 'repo_id',
        'label' => 'Repository ID',
        'optional' => 1,
        'remember' => 0,
      ),
      'id' => 'repo_id',
      'table' => 'versioncontrol_repositories',
      'field' => 'repo_id',
      'relationship' => 'none',
    ),
    'name' => array(
      'operator' => '=',
      'value' => '',
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'name_op',
        'identifier' => 'repo_name',
        'label' => 'Repository Name',
        'optional' => 1,
        'remember' => 0,
      ),
      'case' => 1,
      'id' => 'name',
      'table' => 'versioncontrol_repositories',
      'field' => 'name',
      'relationship' => 'none',
    ),
    'name_1' => array(
      'operator' => '=',
      'value' => '',
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'name_1_op',
        'identifier' => 'commit',
        'label' => 'Commit Name',
        'optional' => 1,
        'remember' => 0,
      ),
      'case' => 1,
      'id' => 'name_1',
      'table' => 'versioncontrol_labels',
      'field' => 'name',
      'relationship' => 'none',
      'operation_type' => '1',
    ),
    'name_2' => array(
      'operator' => '=',
      'value' => '',
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'name_2_op',
        'identifier' => 'branch',
        'label' => 'Branch Name',
        'optional' => 1,
        'remember' => 0,
      ),
      'case' => 1,
      'id' => 'name_2',
      'table' => 'versioncontrol_labels',
      'field' => 'name',
      'relationship' => 'none',
      'operation_type' => '2',
    ),
    'name_3' => array(
      'operator' => '=',
      'value' => '',
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'name_3_op',
        'identifier' => 'tag',
        'label' => 'Tag Name',
        'optional' => 1,
        'remember' => 0,
      ),
      'case' => 1,
      'id' => 'name_3',
      'table' => 'versioncontrol_labels',
      'field' => 'name',
      'relationship' => 'none',
      'operation_type' => '3',
    ),
    'path' => array(
      'operator' => 'starts',
      'value' => '',
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'path_op',
        'identifier' => 'path',
        'label' => 'File path',
        'optional' => 1,
        'remember' => 0,
      ),
      'case' => 1,
      'id' => 'path',
      'table' => 'versioncontrol_item_revisions',
      'field' => 'path',
      'relationship' => 'none',
      'override' => array(
        'button' => 'Override',
      ),
    ),
    'uid' => array(
      'operator' => 'in',
      'value' => '',
      'group' => '0',
      'exposed' => TRUE,
      'expose' => array(
        'use_operator' => 0,
        'operator' => 'uid_op',
        'identifier' => 'username',
        'label' => 'Username',
        'optional' => 1,
        'remember' => 0,
        'reduce' => 0,
      ),
      'id' => 'uid',
      'table' => 'users',
      'field' => 'uid',
      'relationship' => 'none',
    ),
  ));
  $handler->override_option('access', array(
    'type' => 'none',
  ));
  $handler->override_option('cache', array(
    'type' => 'none',
  ));
  $handler->override_option('title', 'Commit Log');
  $handler->override_option('items_per_page', 50);
  $handler->override_option('use_pager', '1');
  $handler->override_option('distinct', 1);
  $handler->override_option('style_plugin', 'versioncontrol_operations');
  $handler->override_option('row_plugin', 'versioncontrol_operations');
  $handler->override_option('exposed_block', TRUE);
  $handler = $view->new_display('page', 'Page', 'page_1');
  $handler->override_option('path', 'commits');
  $handler->override_option('menu', array(
    'type' => 'none',
    'title' => '',
    'description' => '',
    'weight' => 0,
    'name' => 'navigation',
  ));
  $handler->override_option('tab_options', array(
    'type' => 'none',
    'title' => '',
    'description' => '',
    'weight' => 0,
    'name' => 'navigation',
  ));


  $views[$view->name] = $view;


  return $views;
}


