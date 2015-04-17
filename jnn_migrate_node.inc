<?php
/**
 * @file
 * JeepNewsNow node migrations.
 */

/**
 * The JnnTopicMigration populates the "Categories" vocabulary.
 */
class JnnTopicMigration extends JnnMigration {
  public function __construct($arguments) {
    parent::__construct($arguments);
    $this->description = t('Migrate story topics to the "categories" vocabulary in Drupal.');

    // Set up the migration source by creating a query to get the user data we need.
    $query = Database::getConnection('default', 'jeepnewsnow_xoops')
             ->select('xoops_topics', 'xt')
             ->fields('xt', array('topic_id', 'topic_title', 'topic_imgurl'))
             ->orderBy('topic_id', 'ASC');
    $this->source = new MigrateSourceSQL($query);
    
    // Destination, passing in the name of the vocabulary we want to migrate terms into.
    $this->destination = new MigrateDestinationTerm('category');

    // Set up the migration map by defining the source and destination keys for our data.
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'topic_id' => array(
          'type' => 'int',
          'not null' => TRUE,
          'description' => 'Topic ID',
        )
      ),
      MigrateDestinationTerm::getKeySchema()
    );
    
    // Field mappings
    $this->addFieldMapping('name', 'topic_title');

    $this->addFieldMapping('field_category_image', 'topic_imgurl');
    $this->addFieldMapping('field_category_image:source_dir')
      ->defaultValue('public://' . 'legacy/topics');
    $this->addFieldMapping('field_category_image:destination_dir')
      ->defaultValue('public://images/category');
    $this->addFieldMapping('field_category_image:alt', 'topic_title');
    $this->addFieldMapping('field_category_image:title', 'topic_title');
    $this->addFieldMapping('field_category_image:file_replace')
         ->defaultValue('FILE_EXISTS_REUSE');
    $this->addFieldMapping('field_category_image:preserve_files')
      ->defaultValue(TRUE);
    
    $this->addUnmigratedDestinations(array('description', 'parent', 'parent_name', 'format', 'weight',
      'field_category_image:file_class', 'field_category_image:language', 'field_category_image:destination_file'));
  }
}

/**
 * The JnnArticleMigration creates nodes of type "article" from XOOPS "stories".
 */
class JnnArticleMigration extends JnnMigration {
  public function __construct($arguments) {
    parent::__construct($arguments);
    $this->description = t('JeepNewsNow News articles');

    // Set up the migration source by creating a query to get the user data we need.
    $query = Database::getConnection('default', 'jeepnewsnow_xoops')
             ->select('xoops_stories', 'xs')
             ->fields('xs', array('storyid', 'uid', 'title', 'created', 'published', 'expired', 'hometext', 'bodytext', 'counter', 'topicid'))
             //->range(0, 20)
             //->range(1900, 1980)
             //->condition('storyid', '3444', '=')
             //->condition('uid', '1', '=')
             ->orderBy('storyid', 'ASC');
    $this->source = new MigrateSourceSQL($query);
    
    // While we'd love to use the highwater stuff for this class, XOOPS doesn't provide a "last updated" field for us to work with...
    /*
    $this->highwaterField = array(
      'name' => ''
    );
    */
    
    // Destination, passing in the name of the content type we'd like to migrate content into.
    $this->destination = new MigrateDestinationNode('article');

    // Set up the migration map by defining the source and destination keys for our data.
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'storyid' => array(
          'type' => 'int',
          'not null' => TRUE,
          'description' => 'Story ID',
        )
      ),
      MigrateDestinationNode::getKeySchema()
    );

    // Field mappings
    $this->addFieldMapping('title', 'title');
    $this->addFieldMapping('status', 'published');  // See prepareRow() below.
    $this->addFieldMapping('uid', 'uid')
         ->sourceMigration('JnnUser')
         ->defaultValue(1);            // Leverage the JnnUser migration, but if all else fails, set the author to UID=1.
    $this->addFieldMapping('created', 'created');
    $this->addFieldMapping('changed', 'last_updated');  // last_updated is a pseudo-field - see prepareRow().
    $this->addFieldMapping('body', 'bodytext');
    $this->addFieldMapping('body:format')
         ->defaultValue('full_html');  // XOOPS actually uses BBCode markup, we'll handle that in prepareRow().
    $this->addFieldMapping('totalcount', 'counter');
    $this->addFieldMapping('field_category', 'topicid')
         ->sourceMigration('JnnTopic');    // Leverage the JnnTopic migration
    $this->addFieldMapping('field_category:source_type')
         ->defaultValue('tid')
         ->description(t('This is a tricky one, but needs to be set since the incoming terms are keyed.'));
    $this->addFieldMapping('field_category:ignore_case')
         ->defaultValue(TRUE);
    // A value of "1" indicated comments are closed: 
    //   https://api.drupal.org/api/drupal/modules%21comment%21comment.module/constant/COMMENT_NODE_CLOSED/7
    $this->addFieldMapping('comment')
         ->defaultValue(1);

    $this->addUnmigratedDestinations(array('promote', 'sticky', 'revision', 'log', 'language', 'tnid',
      'translate', 'revision_uid', 'is_new', 'body:summary', 'body:language', 'path',
      'field_tags', 'field_tags:source_type', 'field_tags:create_term', 'field_tags:ignore_case',
      'field_image', 'field_image:file_class', 'field_image:language', 'field_image:destination_dir',
      'field_image:destination_file', 'field_image:file_replace', 'field_image:preserve_files',
      'field_image:source_dir', 'field_image:alt', 'field_image:title', 'field_category:create_term'));

    $this->addUnmigratedSources(array('expired', 'hometext'));

    if (module_exists('statistics')) {
      $this->addUnmigratedDestinations(array('daycount', 'timestamp'));
    }
  }
 
  // Act on the source data prior before each row has been migrated.
  public function prepareRow($current_row) {
    //dsm($current_row);

    // XOOPS uses a timestamp for the published status. If it is greater than 0, then set it to 1, else 0.
    //  But, we need the original value of the source "published" field for the Drupal "changed" field, so
    //  we create a psuedo-source field here.
    $current_row->last_updated = $current_row->published;
    if (($current_row->published) > 0) {
      $current_row->published = 1;
    }
    else {
      $current_row->published = 0;
    }

    // Concat and convert hometext and bodytext fields from bbcode
    include_once DRUPAL_ROOT . '/' . drupal_get_path('module', 'bbcode') . '/bbcode-filter.inc';
    $settings = array(
      'bbcode_make_links'       => 1,
      'bbcode_filter_nofollow'  => 1,
      'bbcode_encode_mailto'    => 1,
      'bbcode_paragraph_breaks' => 2,
      'bbcode_debug'            => 0,
    );
    $current_row->bodytext = $current_row->hometext . $current_row->bodytext;
    $current_row->bodytext = _bbcode_filter_process($current_row->bodytext, $settings);
    
    // Rewrite any inline image URLs to new paths.
    $new_path = '/sites/default/files/legacy/news/';
    preg_match_all('/< *img[^>]*src *= *["\']?([^"\']*)/i', $current_row->bodytext, $matches, PREG_SET_ORDER);
    foreach($matches as $key => $match) {
      $current_row->bodytext = str_replace($match[1], $new_path . basename($match[1]), $current_row->bodytext);
    }
  }
  
  // Act on the migrated data after each row has been migrated.
  function complete($entity, $row) {
    // Add the old path as a valid path on the new site via the Redirect module.
    //   Based on http://www.group42.ca/creating_url_redirects_with_migrate_module
    //   The Redirect module has a "Retain query string through redirect" setting.
    //
    // This has to be done in "complete()" rather than "prepareRow()" because we need the nid for the redirect.
    
    // TODO: Move this into the parent class. See http://kevinquillen.com/drupal/2014/07/02/add-redirect-during-migration/
    
    $redirect = new stdClass();
    $redirect->source = 'modules/news/article.php';
    $redirect->source_options = array(
      'query' => array('storyid' => $row->storyid),
    );
    $redirect->redirect = 'node/'. $entity->nid;
    $redirect->redirect_options = array();
    $redirect->status_code = 0;
    $redirect->type = 'redirect';
    $redirect->language = LANGUAGE_NONE;
    redirect_save($redirect);
  }
}

/**
 * The JnnCommentMigration creates comments to Drupal "articles" from XOOPS "stories".
 */
class JnnCommentMigration extends JnnMigration {
  public function __construct($arguments) {
    parent::__construct($arguments);
    $this->description = t('JeepNewsNow comments');

    // Set up the migration source by creating a query to get the user data we need.
    $query = Database::getConnection('default', 'jeepnewsnow_xoops')
      ->select('xoops_xoopscomments', 'xc')
      ->fields('xc', array('com_id', 'com_pid', 'com_rootid', 'com_modid', 'com_itemid', 'com_created', 'com_modified', 
        'com_uid', 'com_ip', 'com_title', 'com_text'))
      //->range(0, 50)
      //->condition('com_id', '25040', '=')
      ->orderBy('com_modified', 'ASC');
    $this->source = new MigrateSourceSQL($query);

    // Comments have a "last updated" timestamp on the source, so we can leverage this if we want to do a continuous migration.
    $this->highwaterField = array(
      'name' => 'com_modified',
      'alias' => 'xc',
      'type' => 'int',
    );
   
    // Destination, passing in the name of the type of comments we want to create.
    $this->destination = new MigrateDestinationComment('comment_node_article');

    // Set up the migration map by defining the source and destination keys for our data.
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'com_id' => array(
          'type' => 'int',
          'not null' => TRUE,
          'description' => 'Comment ID',
        )
      ),
      MigrateDestinationComment::getKeySchema(),
      'default',
      array('track_last_imported' => TRUE)
    );
   
    // Field mappings
    $this->addFieldMapping('nid', 'com_itemid')
         ->sourceMigration('JnnArticle');
    $this->addFieldMapping('uid', 'com_uid')
         ->sourceMigration('JnnUser');
    $this->addFieldMapping('pid', 'com_pid')
         ->sourceMigration('JnnComment');
    $this->addFieldMapping('subject', 'com_title')
         ->description('XOOPS allowed subjects up to 255 characters, Drupal only allows 64, so we must truncate in prepareRow()');
    $this->addFieldMapping('comment_body', 'com_text');
    $this->addFieldMapping('comment_body:format')
         ->defaultValue('filtered_html');
    $this->addFieldMapping('created', 'com_created')
         ->description('Some incoming values are 0, so Drupal sets them to the present date/time. Probably bad.');
    $this->addFieldMapping('changed', 'com_modified')
         ->description('Some incoming values are 0, so Drupal sets them to the present date/time. Probably bad.');
    $this->addFieldMapping('hostname', 'com_ip');
    $this->addFieldMapping('status')
         ->defaultValue(COMMENT_PUBLISHED);
    
    $this->addFieldMapping('thread')
         ->description(t('I think we can ignore this field. From Drupal.org: "An internal string 
           representing a threaded comment\'s position in the hierarchy." XOOPS does not have a field like this, 
           as it uses a different methodology.'))
         ->issueGroup(t('Notes'));
    
    $this->addFieldMapping(NULL, 'com_rootid')
         ->description('I\'m pretty sure this is related to where the comment appears in the overall thread, but it\'s 
           not that important to figure out, so we\'re going to ignore it.')
         ->issueGroup(t('Notes'));
    
    $this->addFieldMapping(NULL, 'com_modid')
         ->description('I think that XOOPS captures the user ID of whomever moderated the comment. Drupal doesn\'t do
           this so we don\'t want it.')
         ->issueGroup(t('Notes'));
    
    $this->addUnmigratedDestinations(array('name', 'mail', 'homepage', 'language', 'comment_body:language'));
  }
  

  // Act on the source data prior before each row has been migrated.
  public function prepareRow($current_row) {
    // XOOPS allowed subjects up to 255 characters, Drupal only allows 64, so we must truncate...
    $current_row->com_title = substr($current_row->com_title, 0, 63);
    
    // Don't import comments that we know are orphans on the source
    $orphan_comment_ids = array('24254', '26434', '26629', '26690', '27090');
    if (in_array($current_row->com_id, $orphan_comment_ids)) {
      return FALSE;
    }

    // Convert comment text from bbcode
    include_once DRUPAL_ROOT . '/' . drupal_get_path('module', 'bbcode') . '/bbcode-filter.inc';
    $settings = array(
      'bbcode_make_links'       => 1,
      'bbcode_filter_nofollow'  => 1,
      'bbcode_encode_mailto'    => 1,
      'bbcode_paragraph_breaks' => 2,
      'bbcode_debug'            => 0,
    );
    $current_row->com_text = _bbcode_filter_process($current_row->com_text, $settings);
  }
  
  // Act on the source data after it has been migrated, but before it has been saved to Drupal.
  public function prepare($comment, stdClass $row) {
    // Some comments have no created and changed dates. Fake them.
    if ((!$row->com_created) || (!$row->com_modified)) {
      // Fake the created and modified dates as the same as their article's created date.
      $story_node = node_load($comment->nid);
      $comment->created = $story_node->created;
      $comment->changed = $story_node->changed;
    }
  }
}

