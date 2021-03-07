<?php
/**
 * @file
 * Support functions for the usermerge module.
 */

/**
 * Implements hook_usermerge_actions_supported().
 */
function uc_file_usermerge_usermerge_actions_supported() {
  return array(
    'uc_file' => t("Migrate a user's Ubercart download files to the new user."),
  );
}


/**
 * Implements hook_usermerge_build_review_form_elements().
 */
function uc_file_usermerge_usermerge_build_review_form_elements($review, $account_properties, $user_to_delete, $user_to_keep) {

  $num_files_to_delete = db_query('
    SELECT COUNT(1)
    FROM {uc_file_users}
    WHERE uid = :uid
    ', array(':uid' => $user_to_delete->uid))
    ->fetchField();

  $num_files_to_keep = db_query('
    SELECT COUNT(1)
    FROM {uc_file_users}
    WHERE uid = :uid
    ', array(':uid' => $user_to_keep->uid))
    ->fetchField();

  if (!$num_files_to_delete && !$num_files_to_keep) {
    return array();
  }

  $review['uc_file'] = array(
    '#tree' => TRUE,
    '#theme' => 'usermerge_data_review_form_table',
    '#title' => t('Ubercart files'),
    '#attributes' => array('no_merge','property_label' => ''),
  );
  $review['uc_file']['files']['options']['user_to_delete'] = array(
    '#type' => 'markup',
    '#markup' => format_plural($num_files_to_delete, '1 file to be reassigned', '@count files to be reassigned'),
  );
  $review['uc_file']['files']['options']['user_to_keep'] = array(
    '#type' => 'markup',
    '#markup' => format_plural($num_files_to_keep, '1 file to be maintained', '@count files to be maintained'),
  );
  return $review;
}


/**
 * Implements hook_usermerge_merge_accounts().
 */
function uc_file_usermerge_usermerge_merge_accounts($user_to_delete, $user_to_keep, $review) {

  $files_to_delete = db_query('
    SELECT *
    FROM {uc_file_users}
    WHERE uid = :uid
    ', array(':uid' => $user_to_delete->uid))
    ->fetchAllAssoc('fid', PDO::FETCH_ASSOC);

  $files_to_keep = db_query('
    SELECT *
    FROM {uc_file_users}
    WHERE uid = :uid
    ', array(':uid' => $user_to_keep->uid))
    ->fetchAllAssoc('fid', PDO::FETCH_ASSOC);

  foreach ($files_to_delete as $fid => $data_to_delete) {
    if (isset($files_to_keep[$fid])) {
      // File already exists for $user_to_keep; construct a merged record.
      $data_to_keep = $files_to_keep[$fid];

      $data_to_keep['granted'] = min($data_to_delete['granted'], $data_to_keep['granted']);

      if ($data_to_delete['expiration'] == 0 || $data_to_keep['expiration'] == 0) {
        $data_to_keep['expiration'] = 0;
      }
      else {
        $data_to_keep['expiration'] = max($data_to_delete['expiration'], $data_to_keep['expiration']);
      }

      $data_to_keep['accessed'] += $data_to_delete['accessed'];
      $data_to_keep['addresses'] = serialize(array_unique(array_merge(
        unserialize($data_to_delete['addresses']), unserialize($data_to_keep['addresses']))));

      if ($data_to_delete['download_limit'] == 0 || $data_to_keep['download_limit'] == 0) {
        $data_to_keep['download_limit'] = 0;
      }
      else {
        $data_to_keep['download_limit'] += $data_to_delete['download_limit'];
      }

      if ($data_to_delete['address_limit'] == 0 || $data_to_keep['address_limit'] == 0) {
        $data_to_keep['address_limit'] = 0;
      }
      else {
        $data_to_keep['address_limit'] += $data_to_delete['address_limit'];
      }

      // remove $data_to_delete
      db_delete('uc_file_users')
        ->condition('fuid', $data_to_delete['fuid'])
        ->execute();

      // update $data_to_keep
      db_update('uc_file_users')
        ->fields($data_to_keep)
        ->condition('fuid', $data_to_keep['fuid'])
        ->execute();
    }
    else {
      // File doesn't exist for $user_to_keep, so just change the uid.
      db_update('uc_file_users')
        ->fields(array('uid' => $user_to_keep->uid))
        ->condition('fuid', $data_to_delete['fuid'])
        ->execute();
    }
  }
}
