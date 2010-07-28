<?php
// $Id$

/**
 * Views Field handler to display a list of items related to a given operation
 *
 * Written following code from 'views_handler_field_user_roles'
 *
 * TODO: Add join options against source_items table/
 */
class versioncontrol_handler_field_operation_items extends views_handler_field_prerender_list {
  
  function option_definition() {
    $options = parent::option_definition();
    $options['type']['default'] = 'ul';
    return $options;
  }
  
  function construct() {
    parent::construct();
    $this->additional_fields['vc_op_id'] = array(
      'table' => 'versioncontrol_operations',
      'field' => 'vc_op_id',
    );
  }
  
  function query() {
    $this->add_additional_fields();
    $this->field_alias = $this->aliases['vc_op_id'];
  }

  function pre_render($values) {
    $this->items = array();
    $vc_op_ids = array();

    foreach ($values as $result) {
      $vc_op_ids[] = $result->{$this->aliases['vc_op_id']};
    }
   
    if($vc_op_ids) {

      // Retrieves the items based on the vc_op_id
      $result = db_query("SELECT voi.vc_op_id, voi.item_revision_id, vir.path 
                          FROM {versioncontrol_operation_items} voi
                          LEFT JOIN {versioncontrol_item_revisions} vir 
                          ON voi.item_revision_id=vir.item_revision_id 
                          WHERE voi.vc_op_id IN (" . implode(", ", $vc_op_ids) . ") 
                          ORDER BY vir.path"); 
      while($item = db_fetch_object($result)) {
        $this->items[$item->vc_op_id][$item->item_revision_id] = array(
          'item_revision_id' => $item->item_revision_id,
          'path' => $item->path,
        );
      }
    }
  }
  
  function render_item($count, $item) {
    return $item['path'];
  }

}
