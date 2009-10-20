<?php
// $Id$

/**
 * @file
 * Funcionts needed to emulate mail handling.
 */

/**
 * Wrapper function to override the default mail handler function.
 * (taken from d7 drupal_web_test_case, hint: rm me on d7)
 *
 * @param  $message
 *   An e-mail message. See drupal_mail() for information on how $message is composed.
 * @return
 *   Returns TRUE to indicate that the e-mail was successfully accepted for delivery.
 */
function drupal_mail_wrapper($message) {
  $captured_emails = variable_get('simpletest_emails', array());
  $captured_emails[] = $message;
  variable_set('simpletest_emails', $captured_emails);

  return TRUE;
}
