<?php

/**
 * Implement hook_migrate_api().
 *
 * API must be set to "2" for the 7.x-2.x branch of Migrate.
 */
function jnn_migrate_migrate_api() {
  $api = array(
    'api' => 2,
    'groups' => array(
      'jnnuser' => array(
        'title' => t('JNN User-related Migrations'),
      ),
      'jnncontent' => array(
        'title' => t('JNN Content-related Migrations'),
      ),
    ),
    'migrations' => array(
      'JnnUser' => array(
        'class_name' => 'JnnUserMigration',
        'group_name' => 'jnnuser',
        'dependencies' => array(
          'JnnAvatar',
        ),
      ),
      'JnnAvatar' => array(
        'class_name' => 'JnnAvatarMigration',
        'group_name' => 'jnnuser',
      ),
      'JnnTopic' => array(
        'class_name' => 'JnnTopicMigration',
        'group_name' => 'jnncontent',
      ),
      'JnnArticle' => array(
        'class_name' => 'JnnArticleMigration',
        'group_name' => 'jnncontent',
        'dependencies' => array(
          'JnnUser',
          'JnnTopic',
        ),
      ),
      'JnnComment' => array(
        'class_name' => 'JnnCommentMigration',
        'group_name' => 'jnncontent',
        'dependencies' => array(
          'JnnUser',
          'JnnArticle',
        ),
      ),
    ),
  );
  return $api;
}
