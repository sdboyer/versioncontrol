<?php
// $Id$

/**
 * Views Field handler to display a list of labels related to a given operation
 *
 * Written following code from 'views_handler_field_user_roles'
 *
 */
class versioncontrol_handler_field_operation_labels extends views_handler_field_prerender_list {
  
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

    foreach($values as $result) {
      $vc_op_ids[] = $result->{$this->aliases['vc_op_id']};
    }
   
    if($vc_op_ids) {

      // query to get all related labels
      $result = db_query("SELECT vol.vc_op_id, vol.label_id, vl.name, vl.type 
                          FROM {versioncontrol_operation_labels} vol 
                          LEFT JOIN {versioncontrol_labels} vl 
                          ON vol.label_id=vl.label_id 
                          WHERE vol.vc_op_id IN (" . implode(", ", $vc_op_ids) . ") 
                          ORDER BY vl.name"); 
                          
      while($item = db_fetch_object($result)) {
        $this->items[$item->vc_op_id][$item->label_id] = array(
          'label_id' => $item->label_id,
          'name' => $item->name,
        );
      }
    }
  }
  
  function render_item($count, $item) {
    return $item['name'];
  }
  
}
