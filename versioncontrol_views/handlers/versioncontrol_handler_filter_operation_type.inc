<?php
// $Id$

/**
 * Views filter handler to allow filtering based on the type of operation
 *
 * Provides text select box rather than requiring the numeric ID
 */

class versioncontrol_handler_filter_operation_type extends views_handler_filter_in_operator {
  
  function get_value_options() {
    require_once drupal_get_path('module', 'versioncontrol') . '/includes/VersioncontrolOperation.php';

    $this->value_options = versioncontrol_views_operation_types();
  } 
}
