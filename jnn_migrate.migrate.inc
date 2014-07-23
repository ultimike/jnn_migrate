<?php

/**
 * Implement hook_migrate_api().
 *
 * API must be set to "2" for the 7.x-2.x branch of Migrate.
 */
function jnn_migrate_migrate_api() {
  $api = array(
    'api' => 2,
    'migrations' => array(
      'JnnUser' => array(
        'class_name' => 'JnnUserMigration',
      ),
      'JnnAvatar' => array(
        'class_name' => 'JnnAvatarMigration'
      ),
      'JnnTopic' => array(
        'class_name' => 'JnnTopicMigration',
      ),
      'JnnArticle' => array(
        'class_name' => 'JnnArticleMigration',
      ),
      'JnnComment' => array(
        'class_name' => 'JnnCommentMigration',
      ),
    ),
  );
  return $api;
}
