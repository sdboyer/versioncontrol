<?php
// $Id$

/**
 * Views Row plugin to display a themed operation object
 *
 */
class versioncontrol_plugin_row_operation extends views_plugin_row {
  var $base_table = 'versioncontrol_operation';
  var $base_field = 'vc_po_id';
  var $operations = array();


  // queries and generates the operation objects based on the found vc_op_ids
  function pre_render($result) {
    $constraints = array();
    foreach ($result as $row) {
      $constraints['vc_op_ids'][] = $row->vc_op_id;
    }
    $this->operations = VersioncontrolOperationCache::getInstance()->getOperations($constraints);
  }

  // returns themed version of this vc_op_id
  function render($row) {
    $vc_op_id = $row->{$this->field_alias};

    if (!is_numeric($vc_op_id)) {
      return;
    }
    $operation = $this->operations[$vc_op_id];

    return theme('versioncontrol_views_operation', $operation);

  }
  
}


