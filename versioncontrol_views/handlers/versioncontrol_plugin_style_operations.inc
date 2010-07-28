<?php
// $Id$

/**
 * Views style plugin to display a date grouped list of items
 *
 * The default handlers cannot group if the row plugin doesn't support fields
 * so the commit log is generated using this.
 *
 */
class versioncontrol_plugin_style_operations extends views_plugin_style {
  var $formatting = "M d, Y"; //TODO: Make an option
  function query() {
    parent::query();
    
    $this->view->query->add_field('versioncontrol_operations', 'date');
  }
  
  function render() {
    if(empty($this->row_plugin)) {
      vpr('versioncontrol_plugin_style_operations: Missing row plugin');
    }
    
    $group_alias = '';
    foreach ($this->view->query->fields as $alias => $data) {
      if($data['field'] == 'date') {
        $group_alias = $alias;
      }
    }

    $groups = array();

    foreach ($this->view->result as $index => $row) {
      $date = format_date($row->{$group_alias}, 'custom', $this->formatting);
      $groups[$date][] = array($this->row_plugin->render($row)); 
    }
    
    foreach ($groups as $date => $rows) {
        $groups[$date] = theme('table', array(), $rows);
    }
    
    return theme($this->theme_functions(), $this->view, $this->options, $groups, $title);
  }
  
}
