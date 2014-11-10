<?php

/**
 * Class JnnMigration
 *
 * An abstract class that extends the basic Migrate DynamicMigration class and encapsulates
 *  information specific to all of the JeepNewsNow migration classes, including the database
 *  connection, team members, and issue URL pattern.
 */
abstract class JnnMigration extends DynamicMigration {
  public function __construct($group = null) {
    // Always call the parent constructor first for basic setup
    parent::__construct($group);

    // This is the database connection information for the source database. 
    //
    // The parameters are: the database key, the database target name 
    //   (often used with master/slave database replication), and the database
    //   connection info, which is dependent on the database driver. 
    //
    // A good way of ensuring the connection information is correct is to duplicate the
    //   DB connection info from your site's setting.php file, and just changing the 
    //   'database' value to the source database name (this assumes that your Drupal 7
    //   database and the source database are on the same database server).
    Database::addConnectionInfo('jeepnewsnow_xoops', 'default', array(
      'driver' => 'mysql',
      'database' => 'jeepnewsnow_xoops_subset',
      'username' => 'root',
      'password' => 'root',
      'host' => 'localhost',
      'prefix' => '',
      'port' => '3306',
    ));
    /*
    // Acquia Dev Desktop 2
    Database::addConnectionInfo('jeepnewsnow_xoops', 'default', array(
      'driver' => 'mysql',
      'database' => 'jeepnewsnow_xoops_subset',
      'username' => 'root',
      'password' => '',
      'host' => '127.0.0.1',
      'prefix' => '',
      'port' => '33067',
    ));
    */
    // With migrate_ui enabled, migration pages will indicate people involved in
    // the particular migration, with their role and contact info. We default the
    // list in the shared class; it can be overridden for specific migrations.
    $this->team = array(
      new MigrateTeamMember('Mike Anello', 'manello@gmail.com', t('Migrator')),
    );

    // Individual mappings in a migration can be linked to a ticket or issue
    // in an external tracking system. Define the URL pattern here in the shared
    // class with ':id:' representing the position of the issue number, then add
    // ->issueNumber(1234) to a mapping.
    $this->issuePattern = 'http://projects.drupaleasy.com/node/:id:';
  }
}

/**
 * Class JnnUserMigration
 *
 * Used to migrate users.
 */
class JnnUserMigration extends JnnMigration {
  public function __construct() {
    // Call the parent constructor and put this migration in a migration group.
    parent::__construct(MigrateGroup::getInstance('jnn_users'));
    
    $this->description = t('JeepNewsNow Users from the wonderful XOOPS CMS.');
    
    // Ensure the avatar image files are migrated before the users.
    $this->dependencies = array('JnnAvatar');

    // Set up the migration source by creating a query to get the user data we need.
    $query = Database::getConnection('default', 'jeepnewsnow_xoops')
             ->select('xoops_users', 'xu')
             ->fields('xu', array('uid', 'name', 'uname', 'email', 'user_regdate', 
                                  'last_login', 'user_mailok', 'pass', 'timezone_offset',
                                  'user_sig', 'user_avatar'));
    $this->source = new MigrateSourceSQL($query);
    
    // Set up the migration destination. Since XOOPS uses md5 password hashes, we can take advantage of it.
    $this->destination = new MigrateDestinationUser(array('md5_passwords' => TRUE));
    
    // Set up the migration map by defining the source and destination keys for our data.
    $this->map = new MigrateSQLMap($this->machineName,
        array(
          'uid' => array(
            'type' => 'int',
            'not null' => TRUE,
            'description' => 'Xoops User ID.'
          )
        ),
        MigrateDestinationUser::getKeySchema()
    );
    
    // Mapped fields (destination, source)
    $this->addFieldMapping('name', 'uname')
         ->dedupe('users', 'name');  // Ensure we don't have any duplicate usernames.
    $this->addFieldMapping('created', 'user_regdate');
    $this->addFieldMapping('mail', 'email');
    $this->addFieldMapping('access', 'last_login');
    $this->addFieldMapping('pass', 'pass');
    $this->addFieldMapping('status')
         ->defaultValue(1);
    $this->addFieldMapping('timezone')
         ->defaultValue('America/New_York')
         ->description(t('Can we translate a timezone like "-5.0" to a D7 timezone string?'))
         ->issueNumber(1234)
         ->issueGroup(t('Incomplete'));

    $this->addFieldMapping('signature', 'user_sig');
    $this->addFieldMapping('signature_format')
         ->defaultValue('plain_text');  // This is the text format we want to assign to all signatures.

    // Map the uid to the picture field because the JnnAvatar migration translates it 
    //   into the proper file id. The Drupal 7 "picture" field expects a File ID (fid).
    $this->addFieldMapping('picture', 'uid')
         ->sourceMigration('JnnAvatar')
         ->description('Via the JnnAvatar map table.');

    // Unmapped source fields
    $this->addUnmigratedSources(array('name', 'user_mailok', 'timezone_offset', 'user_avatar'));

    // Unmapped destination fields
    $this->addUnmigratedDestinations(array('theme', 'login', 'init', 'data', 'language', 'is_new', 'roles', 'role_names', 'path'));
  }

}

/**
 * Class JnnAvatarMigration
 *
 * Used to migrate user avatars.
 */
class JnnAvatarMigration extends JnnMigration {
  public function __construct() {
    // Call the parent constructor and put this migration in a migration group.
    parent::__construct(MigrateGroup::getInstance('jnn_users'));
    $this->description = t('Migrates non-blank user avatar images.');

    // Set up the migration source by creating a query to get the user data we need.
    // Raw SQL: SELECT xu.uid, xu.user_avatar FROM xoops_users xu WHERE xu.user_avatar
    //          != 'blank.gif';
    $query = Database::getConnection('default', 'jeepnewsnow_xoops')
      ->select('xoops_users', 'xu')
      ->condition('user_avatar', 'blank.gif', '!=')
      ->fields('xu', array('uid', 'user_avatar'));
    $this->source = new MigrateSourceSQL($query);

    // Set up the migration destination
    $this->destination = new MigrateDestinationFile();

    // Set up the migration map by defining the source and destination keys for our data.
    //   On the source side, the ID is the User ID (because there is nothing like a "file
    //   ID" in the source table). On the destination side, Drupal's "File ID" ("fid") 
    //   will be used (MigrateDestinationFile::getKeySchema() does this automatically
    //   for us).
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'uid' => array(
          'type' => 'int',
          'not null' => TRUE,
          'description' => 'Xoops User ID.'
        )
      ),
      MigrateDestinationFile::getKeySchema()
    );

    // TODO: Add a check to ensure the file exists. See http://kevinquillen.com/drupal/2014/07/08/media-migrate-trick/

    // Drupal core picture field (not using Field API) simply links from the users table to a file
    //   in the file_managed table, so all we need to to is upload our avatar images to there.
    //   Download and enable the File Entity module to add a "Files" page under the
    //   main admin "Content" area.
    //$this->addFieldMapping('destination_field', 'source_field');
    
    // Don't map the uid, instead, let Drupal assign the uid=0 to the file.
    //$this->addFieldMapping('uid', 'uid');
    $this->addFieldMapping('value', 'user_avatar');

    $this->addFieldMapping('source_dir')
      ->defaultValue('public://' . 'legacy/avatars');
    $this->addFieldMapping('destination_dir')
      ->defaultValue('public://' . variable_get('user_picture_path', 'pictures'))
      ->description('Using default pictures file location.');
    $this->addFieldMapping('file_replace')
      ->defaultValue('FILE_EXISTS_REUSE');  // If the file already exists in the destination, reuse it.
    $this->addFieldMapping('preserve_files')
      ->defaultValue(TRUE);   // Don't delete files on rollbacks.
    // All source fields mapped

    // Unmapped destination fields
    $this->addUnmigratedDestinations(array('destination_file', 'timestamp'));
  }

}
