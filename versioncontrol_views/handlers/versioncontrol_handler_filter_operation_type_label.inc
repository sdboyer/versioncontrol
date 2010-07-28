<?php
// $Id$

/**
 * Views filter handler to filter by label BUT also allows limitting based on type
 *
 * Uses a standard string handler but adds the type to the options field
 * This is primarily to allow the exposed filter to have a link like tag= or branch=
 * and be able to filter properly. 
 * 
 */
class versioncontrol_handler_filter_operation_type_label extends views_handler_filter_string {
  
  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    
    $options = array(
      0 => t("All"),
    ) + versioncontrol_views_operation_types();
    
    $form['operation_type'] = array(
      '#type' => 'radios',
      '#title' => t('Operation type'),
      '#default_value' => $this->options['operation_type'],
      '#options' => $options,
    );
    
  }

  function query() {
    parent::query();
    $type = $this->options['operation_type'];
    if (!empty($type)) {
      $this->query->add_where($this->options['group'], $this->table_alias . ".type = %d", $type);
    }
  }
  
  
}
