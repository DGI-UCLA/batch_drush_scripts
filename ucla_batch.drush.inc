<?php

/*
 * This script needs to be updated by namespacing the functions
 * as is there could be function name conflicts with other drush scripts
 * @todo: add logging before a full ingest is done
 */

//drush hook
function drush_ucla_batch_la_times_batch($collection_pid, $relationship, $tiff_input_directory, $mods_input_directory) {
  drush_print('Checking input.');
  module_load_include('inc', 'fedora_repository', 'CollectionClass');
  set_time_limit(0); //this could be long so lets have infinite time on our hands

  drush_print('Current working directory ' . getcwd());
  if (isset($collection_pid)) {
    drush_print("Used collection pid of " . $collection_pid);
  }
  else {
    drush_print("No collection pid specified");
    return;
  }
  if (isset($relationship)) {
    drush_print("and relationship of " . $relationship);
  }
  else {
    drush_print("No relationship specified");
    return;
  }
  if (!isset($tiff_input_directory)) {
    drush_print("No image intput path specified.");
    return;
  }
  if (!isset($mods_input_directory)) {
    drush_print("No metadata intput path specified.");
    return;
  }

  la_times_doAction($collection_pid, $relationship, $tiff_input_directory, $mods_input_directory);
}

/**
 * ingest la times photos
 * @param <type> $pid
 * @param <type> $relationship
 * @param <type> $input_path
 */
function la_times_doAction($collection_pid, $relationship, $tiff_input_directory, $mods_input_directory) {

  drush_print('Starting ingest');
  drush_print("Tiff input path: " . $tiff_input_directory);
  drush_print("Mods input path: " . $mods_input_directory);

  $mods_directories = array();
  $tiff_files = array();
  $mods_directories = scandir($mods_input_directory);
  $tiff_files = scandir($tiff_input_directory);

  //loop through the mods directories, the acutal files are in subdirs
  foreach ($mods_directories as $mods_directory) {
    $mods_files = Null;

    if ($mods_directory === '.' || $mods_directory === '..') {
      continue;
    }

    //this dir path is a sub dir of $mods_input_directory
    $mods_directory_path = $mods_input_directory . '/' . $mods_directory;

    drush_print('subdir: ' . $mods_directory_path);
    //traverse the sub directories that have mods in them.
    if (is_dir($mods_directory_path)) {
      drush_print('Searching for MODS in directory: ' . $mods_directory);
      $mods_files = scandir($mods_directory_path);
       drush_print('files in dir: ' . implode($mods_files));
      foreach ($mods_files as $mods_file) {
        if ($mods_file === '.' || $mods_file === '..') {
          continue;
        }

        //handle both tiff and tif file extension. explitive naming conventions
        $tiff_file = FALSE;
        $tiff_file = array_search(substr($mods_file, 0, strlen($mods_file) - 4) . '.tif', $tiff_files);
        drush_print('mods file name: ' . $mods_file);
        drush_print('tiff file name: ' . substr($mods_file, 0, strlen($mods_file) - 4));

        if ($tiff_file === FALSE) {
          $tiff_file = array_search(substr($mods_file, 0, strlen($mods_file) - 4) . '.tiff', $tiff_files);
        }

        if ($tiff_file !== FALSE) {
          //find matching tiff file
          $tiff_file_path = $tiff_input_directory . '/' . $tiff_file;
          $mods_file_path = $mods_directory_path . '/' . $mods_file;

          //run ingest on mods/tiff pair
          la_times_create_object($tiff_file_path, $mods_file_path, $collection_pid, $relationship);
        }
      }
    }
  }
}

/**
 * This function ingests per an object
 */
function la_times_create_object($orginal_file, $mods_file, $collection_pid, $relationship) {
  drush_print('Ingesting: ' . $mods_file);
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $xml = simplexml_load_string(file_get_contents($mods_file));
  $xml->registerXPathNamespace('ns', "http://www.loc.gov/mods/v3");
  $label = $xml->xpath('//ns:titleInfo/ns:title');
  $label = (string) $label[0]; //this mods only has the one title
  drush_print('using label ' . $label);
  $output = array();
  //uses fedora settings taken for islandora drupal settings
  $new_pid = Fedora_Item::get_next_PID_in_namespace('ucla'); //hard coding pid namespace could be passed as a variable if needed
  drush_print("creating object with pid " . $new_pid);
  $object = Fedora_Item::ingest_new_item($new_pid, 'A', $label); //create a new skeleton object
  drush_print("created object $new_pid now adding datastreams");

  if ($object->add_datastream_from_string($xml->asXML(), 'MODS', 'MODS', 'text/xml', 'X', 'added MODS Stream')) {
    drush_print('added MODS stream to ' . $new_pid);
  }
  else {
    drush_print('Failed adding MODS stream to ' . $new_pid . " using url $mods_file");
  }

  //add image datastreams
  if ($orginal_file) {
    exec('kdu_compress -i "' . $orginal_file . '" -o "' . $orginal_file . '.jp2' . '" -rate 0.5 Clayers=1 Clevels=7 "Cprecincts={256,256},{256,256},{256,256},{128,128},{128,128},{64,64},{64,64},{32,32},{16,16}" "Corder=RPCL" "ORGgen_plt=yes" "ORGtparts=R" "Cblk={32,32}" Cuse_sop=yes', $output, $returnValue);
    if($returnValue =='0'){
      $object->add_datastream_from_file($orginal_file.'.jp2','JP2','JP2', 'image/jp2','M','added JP2 stream');
    }
    exec("convert  -resize '750x750' \"{$orginal_file}\"[0]  \"$orginal_file-med.jpg\"", $output, $returnValue);
    if($returnValue =='0'){
       $object->add_datastream_from_file($orginal_file.'-med.jpg','JPG','JPG', 'image/jpeg','M','added JPG stream');
    }
    exec("convert {$orginal_file}[0] -thumbnail x2000 -thumbnail x450  -resize '250x300' -fuzz 1% -trim +repage -gravity center -crop 200x200+0+0 +repage -format jpg -quality 100  $orginal_file-tn.jpg", $output, $returnValue);
    if($returnValue =='0'){
       $object->add_datastream_from_file($orginal_file.'-tn.jpg','TN','TN', 'image/jpeg','M','added JPG stream');
    }
    if ($object->add_datastream_from_file($orginal_file, 'TIF', 'TIF', 'image/tif', 'M', 'added TIF Stream')) {
      drush_print('added TIF stream to ' . $new_pid);
    }
    else {
      drush_print('Failed adding TIF stream to ' . $new_pid);
    }
  }

  $object->add_relationship($relationship, $collection_pid);
  //islandora:sp_large_image_cmodel
  $object->add_relationship('hasModel', 'islandora:sp_large_image_cmodel', FEDORA_MODEL_URI);
}

function endswith($string, $test) {
  $strlen = strlen($string);
  $testlen = strlen($test);
  if ($testlen > $strlen)
    return false;
  return substr_compare($string, $test, -$testlen) === 0;
}

function ucla_batch_drush_command() {
  $items = array();

  $items['la_times_batch'] = array(
  'description' => "gets a list of files from a web accessible location and ingest them into Fedora.  This is currently hard coded to look for files that end with .tif and for each tif will log for a .xml and a .jpg and add these as datastreams",
  'arguments' => array(
  'collection_pid' => 'The pid of the collection object, this is the collection the new objects will be related to',
  'relationship' => 'The relationship the objects have to the collection object.  For example isMemberOf is a common relationship.',
  'tiff_input_directory' => 'The path to the tiff files (absolute file system path, /var/www/http/drupal/sites/default/files).',
  'mods_input_directory' => 'The path to the mods files directories parent (absolute file system path, /var/www/http/drupal/sites/default/files).',
  'webserver_path_to_file' => 'The path relative to drupal host so Fedora can find the file (http://localhost/drupal/files)'
  ),
  'examples' => array(
  'drush -u 1 la_times_batch islandora:collection isMemberOfCollection /var/www/html/drupal/sites/default/files/UCLA-LA-Times/latimes_images /var/www/html/drupal/sites/default/files/UCLA-LA-Times/latimes_mods',
  ),
  'aliases' => array('latimesbatch'),
  'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN, // we can pass in users id on the command line using drush -u.
  );

  return $items;
}
?>
