<?php

/*
 * This script needs to be updated by namespacing the functions
 * as is there could be function name conflicts with other drush scripts
 * @todo: add logging before a full ingest is done
 * @todo: explore timeouts
 */

//drush hook

function drush_la_times_batch($collection_pid, $relationship, $input_path) {
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

  la_times_doAction($collection_pid, $relationship, $input_path);
}

/**
 * ingest la times photos
 * @param <type> $pid
 * @param <type> $relationship
 * @param <type> $input_path
 */
function la_times_doAction($collection_pid, $relationship, $input_path) {
  drush_print("input path = " . $input_path);
  $files = array();
  $dirs = scandir($input_path);
  foreach ($dirs as $dir) {
    if ($dir === '.' || $dir === '..') {
      continue;
    }
    $files = scandir($input_path . "/" . $dir);
    $path = $input_path . '/' . $dir . '/';
    la_times_start_ingest($files, $path, $collection_pid, $relationship);
  }
}

function la_times_start_ingest($files, $path, $collection_pid,$relationship) {
  $width = '100';
  $height = '100';
  $mods_file = $path . 'ExportMods.xml';
  foreach ($files as $file) {
    if ($file === '.' || $file === '..') {
      continue;
    }
    else {
      drush_print('found file ' . $path . $file);
      if (endswith($file, ".tiff") || endswith($file, ".tif")) {
        $orginal_file = $path . $file;
      }
    }
  }
  la_times_create_object($orginal_file, $mods_file, $collection_pid, $relationship);
}

function la_times_create_object($orginal_file, $mods_file, $collection_pid, $relationship) {
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
    exec("convert  -resize 800 \"{$orginal_file}\"[0]  \"$orginal_file-med.jpg\"", $output, $returnValue);
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
?>
