<?php
// $Id$

/**
 * Views filter handler that pulls the formatted name from the backend object
 * instead of just using the short name. The just name it look slightly nicer
 * and checkboxes are easier.
 *
 */
class versioncontrol_handler_filter_repo_backend extends views_handler_filter_in_operator {
  function get_value_options() {
    if (!isset($this->value_options)) {
      $this->value_title = t('VCS Backends');
      $types = versioncontrol_get_backends();
      foreach ($types as $shorttype => $backend_obj) {
        $options[$shorttype] = t($backend_obj->name);
      }
      $this->value_options = $options;
    }
  }
}
