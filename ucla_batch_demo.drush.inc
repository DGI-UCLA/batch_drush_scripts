<?php

/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */

//drush hook

function ucla_batch_demo_drush_command() {
  $items = array();

  $items['ucla_batch_demo'] = array(
    'description' => "gets a list of files from a web accessible location and ingest them into Fedora.  This is currently hard coded to look for files that end with .tif and for each tif will log for a .xml and a .jpg and add these as datastreams",
    'arguments' => array(
      'collection_pid' => 'The pid of the collection object, this is the collection the new objects will be related to',
      'relationship' => 'The relationship the objects have to the collection object.  For example isMemberOf is a common relationship.',
      'input_path' => 'The path to the files (absolute file system path, /var/www/http/drupal/sites/default/files).',
      'webserver_path_to_file' => 'The path relative to drupal host so Fedora can find the file (http://localhost/drupal/files)'
    ),
    'examples' => array(
      'drush ucla_batch_demo islandora:collection isMemberOfCollection /Applications/MAMP/htdocs/drupal/AIDS_posters http://localhost/drupal/AIDS_posters/',
    ),
    'aliases' => array('uclabatchdemo'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN, // we can pass in users id on the command line using drush -u.
  );

  return $items;
}

//drush hook
function drush_ucla_batch_demo($collection_pid, $relationship, $input_path, $webserver_path_to_file) {
  module_load_include('inc', 'fedora_repository', 'CollectionClass');
  drush_print('Current working directory ' . getcwd());
  if (isset($collection_pid)) {
    drush_print("\n Used collection pid of" . $collection_pid . "\n");
  }
  else {
    drush_print("no collection pid specified");
    return;
  }
  if (isset($relationship)) {
    drush_print("\n and relationship of" . $relationship);
  }
  else {
    drush_print("\n no relationship specified");
    return;
  }
  if (!isset($input_path)) {
    drush_print("\n no output path specified");
    return;
  }
  if (!isset($webserver_path_to_file)) {
    drush_print("\n no webserver path specified");
    return;
  }
  doAction($collection_pid, $relationship, $input_path, $webserver_path_to_file);
}

//just a function
function doAction($pid, $relationship, $input_path, $webserver_path_to_file) {
  drush_print("input path = " . $input_path);
  $files = array();
  if ($handle = opendir($input_path)) {
    while (false !== ($file = readdir($handle))) {
      if (endswith($file, ".tif")) {
        drush_print($file);
        $files[] = $file; //populate array we could probably just call our create object function here but if we needed more complicated processing we could pass the array around
      }
    }
    closedir($handle);
  }

  foreach ($files as $tif_file) {
    create_object($pid, $relationship, $tif_file, $webserver_path_to_file);
  }
}

function create_object($pid, $relationship, $tif_file, $webserver_path_to_file) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  //uses fedora settings taken for islandora drupal settings
  $new_pid = Fedora_Item::get_next_PID_in_namespace();//we did not define a namespace so it will use the defaul
  drush_print("creating object with pid " . $new_pid);
  $object = Fedora_Item::ingest_new_item($new_pid, 'A', $tif_file);//create a new skeleton object
  drush_print("created object $new_pid now adding datastreams");
  $file_prefix = substr($tif_file,0,strlen($tif_file)-4);
  if ($object->add_datastream_from_url(url("$webserver_path_to_file/$file_prefix.xml"), 'MODS', 'MODS', 'text/xml', 'M', 'added MODS Stream')) {
    drush_print('added MODS stream to ' . $new_pid);
  }
  else {
    drush_print('Failed adding MODS stream to ' . $new_pid." using url $webserver_path_to_file/$file_prefix.xml");
  }
  if ($object->add_datastream_from_url(url("$webserver_path_to_file/$file_prefix.tif"), 'TIF', 'TIF', 'image/tif', 'M', 'added TIF Stream')) {
    drush_print('added TIF stream to ' . $new_pid);
  }
  else {
    drush_print('Failed adding TIF stream to ' . $new_pid);
  }
  if ($object->add_datastream_from_url(url("$webserver_path_to_file/$file_prefix.jpg"), 'JPG', 'JPG', 'image/jpg', 'M', 'added JPG Stream')) {
    drush_print('added JPG stream to ' . $new_pid);
  }
  else {
    drush_print('Failed adding JPG stream to ' . $new_pid);
  }
  
}


function endswith($string, $test) {
  $strlen = strlen($string);
  $testlen = strlen($test);
  if ($testlen > $strlen)
    return false;
  return substr_compare($string, $test, -$testlen) === 0;
}

?>
