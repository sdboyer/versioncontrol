<?php
// $Id$
/**
 * @file
 * FakeVCS backend for Version Control API -
 * An example module illustrating how to write a VCS backend.
 *
 * This file contains the administrative user interface customizations
 * for accounts and repositories.
 *
 * Copyright 2006, 2007 Derek Wright ("dww" , http://drupal.org/user/46549)
 * Copyright 2007, 2008 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

define('VERSIONCONTROL_FAKEVCS_MIN_PASSWORD_LENGTH', 5);

/**
 * Implementation of hook_form_alter(): Add elements to various
 * administrative forms that the Version Control API provides.
 */
function versioncontrol_fakevcs_form_alter(&$form, $form_state, $form_id) {
  if ($form['#id'] == 'versioncontrol-repository-form' && $form['#vcs'] == 'fakevcs') {
    versioncontrol_fakevcs_repository_admin_form_alter($form, $form_state, $form_id);
  }
  elseif ($form['#id'] == 'versioncontrol-account-form' && $form['#vcs'] == 'fakevcs') {
    versioncontrol_fakevcs_account_form_alter($form, $form_state, $form_id);
  }
}

/**
 * Add CVS specific elements to the add/edit repository form.
 */
function versioncontrol_fakevcs_repository_admin_form_alter(&$form, $form_state, $form_id) {
  $repository = isset($form['#repository'])? $form['#repository']: null;

  $form['#versioncontrol_fakevcs'] = TRUE;

  $form['repository_information']['root']['#description'] = t(
    'The location of the repository\'s root directory. Examples: /path or ssh+fakevcs://path.'
  );
  $form['repository_information']['update_method'] = array(
    '#type' => 'radios',
    '#title' => t('Update method'),
    '#description' => t('Automatic log retrieval requires cron.'),
    '#default_value' => isset($repository)
                        ? $repository['fakevcs_specific']['update_method']
                        : VERSIONCONTROL_FAKEVCS_UPDATE_CRON,
    '#weight' => 10,
    '#options' => array(
      VERSIONCONTROL_FAKEVCS_UPDATE_CRON => t('Automatic log retrieval.'),
      VERSIONCONTROL_FAKEVCS_UPDATE_SCRIPT => t('Use external script to insert data.'),
    ),
  );
}

/**
 * Implementation of hook_versioncontrol_repository_submit():
 * Extract repository data from the repository editing/adding form's
 * submitted values, and add it to the @p $repository array. Later, that array
 * will be passed to hook_versioncontrol_repository() as part of the repository
 * insert/update procedure.
 */
function versioncontrol_fakevcs_versioncontrol_repository_submit(&$repository, $form, $form_state) {
  if (!isset($form['#versioncontrol_fakevcs'])) {
    return;
  }
  $repository['fakevcs_specific']['update_method'] = $form_state['values']['update_method'];
}

/**
 * Implementation of hook_versioncontrol_alter_repository_list():
 * Add CVS specific columns into the list of CVS repositories.
 * By changing the @p $header and @p $rows_by_repo_id arguments,
 * the repository list can be customized accordingly.
 *
 * @param $vcs
 *   The unique string identifier for the version control system that
 *   the passed repository list covers.
 * @param $repositories
 *   An array of repositories of the given version control system.
 *   Array keys are the repository ids, and array values are the
 *   repository arrays like returned from versioncontrol_get_repository().
 * @param $header
 *   A list of columns that will be passed to theme('table').
 * @param $rows_by_repo_id
 *   An array of existing table rows, with repository ids as array keys.
 *   Each row already includes the generic column values, and for each row
 *   there is a repository with the same repository id given in the
 *   @p $repositories parameter.
 */
function versioncontrol_fakevcs_versioncontrol_alter_repository_list($vcs, $repositories, &$header, &$rows_by_repo_id) {
  if ($vcs != 'fakevcs') {
    return;
  }
  $header[] = t('Update method');

  foreach ($rows_by_repo_id as $repo_id => $row) {
    switch ($repositories[$repo_id]['fakevcs_specific']['update_method']) {
      case VERSIONCONTROL_FAKEVCS_UPDATE_SCRIPT:
        $rows_by_repo_id[$repo_id][] = t('external script');
        break;
      case VERSIONCONTROL_FAKEVCS_UPDATE_CRON:
        $rows_by_repo_id[$repo_id][] = t('logs (!fetch)', array(
          '!fetch' => l(t('fetch now'), 'admin/project/versioncontrol-repositories/update/'. $repo_id)
        ));
        break;
      default:
        break;
    }
  }
}


/**
 * Add FakeVCS specific elements to the edit/register user account form.
 */
function versioncontrol_fakevcs_account_form_alter(&$form, $form_state, $form_id) {
  $form['#versioncontrol_fakevcs'] = TRUE;

  if (empty($form['#original_username'])) { // creating the account
    $description = t('Choose a password to access the FakeVCS repository with.');
  }
  else { // editing the account
    $description = t('To change the current FakeVCS password, enter the new password in both fields.');
  }
  $form['account']['account_password'] = array(
    '#type' => 'password_confirm',
    '#title' => t('FakeVCS password'),
    '#description' => $description,
    '#weight' => 10,
  );
  $form['#validate'][] = 'versioncontrol_fakevcs_account_form_validate';
}

/**
 * Additional validation for the edit/register user account form.
 */
function versioncontrol_fakevcs_account_form_validate($form, &$form_state) {
  if (!empty($form['#original_username']) && empty($form_state['values']['account_password'])) {
    return; // The (existing) user didn't change the password.
  }
  elseif (drupal_strlen($form_state['values']['account_password']) < VERSIONCONTROL_FAKEVCS_MIN_PASSWORD_LENGTH) {
    form_set_error('account_password', t('The FakeVCS password you have chosen is too short (it must be at least !min characters long).', array('!min' => VERSIONCONTROL_FAKEVCS_MIN_PASSWORD_LENGTH)));
  }
}

/**
 * Implementation of hook_versioncontrol_account_submit():
 * Extract account data from the account edit/register form's submitted
 * values, and add it to the @p $additional_data array. Later, that array
 * will be passed to hook_versioncontrol_account() as part of the account
 * insert/update procedure.
 */
function versioncontrol_fakevcs_versioncontrol_account_submit(&$additional_data, $form, $form_state) {
  if (!isset($form['#versioncontrol_fakevcs']) || empty($form_state['values']['account_password'])) {
    return;
  }
  $additional_data['fakevcs_specific']['password'] = crypt($form_state['values']['account_password']);
}
