<?php
// $Id$

/**
 * Theme the subsections of the VCS Operation and save values for rendering
 * Into the main theme file
 */
function template_preprocess_versioncontrol_views_operation(&$variables) {
  
  $operation = $variables['operation'];
  $operation_items = $operation->getItems();
  $variables['operation_items'] = $operation_items;
  
  $variables['username'] = theme('versioncontrol_account_username', $operation->uid, 
                $operation->author, $operation->repository, array('format' => 'html'));
  
  $variables['id'] = theme('commitlog_revision_identifier', $operation, 'html');
  $variables['repository_name'] = theme('commitlog_repository', $operation->repository, 'html');
  $variables['time'] = format_date($operation->date, 'custom', 'H:i');


  $variables['items'] = theme('commitlog_operation_items', $operation, $operation_items, 'html');
  $variables['description'] = theme('commitlog_operation_message', $operation, 'html');
  $variables['title'] = theme('commitlog_operation_caption', $variables, array('format' => 'html'));

  $variables['items'] = versioncontrol_views_replace_commitlog($variables['items']);
  $variables['description'] = versioncontrol_views_replace_commitlog($variables['description']);
  $variables['title'] = versioncontrol_views_replace_commitlog($variables['title']);
}

/**
 * Rewrite links to have the proper arguments
 */
function versioncontrol_views_replace_commitlog($in) {
  $tmp = preg_replace('/\/commitlog\?/', '/commits?', $in);
  
  $tmp = preg_replace('/(\?|&amp;)paths=/', '${1}path=', $tmp);
  $tmp = preg_replace('/(\?|&amp;)usernames=/', '${1}username=', $tmp);
  
  $tmp = preg_replace('/(\?|&amp;)branches=/', '${1}branch=', $tmp);
  $tmp = preg_replace('/(\?|&amp;)tags=/', '${1}tag=', $tmp);
  $tmp = preg_replace('/(\?|&amp;)commits=/', '${1}commit=', $tmp);
  
  $tmp = preg_replace('/(\?|&amp;)repos=/', '${1}repo_id=', $tmp);
  $tmp = preg_replace('/(\?|&amp;)repo=/', '${1}repo_id=', $tmp);
  
  $tmp = preg_replace('/(\?|&amp;)id=/', '${1}op_id=', $tmp);
  $tmp = preg_replace('/(\?|&amp;)ids=/', '${1}op_id=', $tmp);
  
  
  return $tmp;
}


