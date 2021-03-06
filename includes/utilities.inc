<?php

/**
 * @file
 * Utility functions used by the Islandora Usage Stats Charts module.
 */

/**
 * Populate the usage statistics block for non-collection objects.
 */
function islandora_usage_stats_charts_generate_stats($pid) {
  drupal_add_js('misc/collapse.js');
  drupal_add_js('misc/form.js');
  $access_log_id = islandora_usage_stats_charts_get_object_access_id($pid);

  $js_url = variable_get('islandora_usage_stats_charts_javascript_url', 'https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.2/Chart.bundle.js');
  drupal_add_js($js_url, 'external');

  // Get usage stats going back X months and group them by month.
  $num_months = variable_get('islandora_usage_stats_charts_months_to_show', '6');
  $ago = strtotime('-' . $num_months . ' months', time());
  $views_result = db_select('islandora_usage_stats_object_access_log')
    ->fields('islandora_usage_stats_object_access_log', array('time'))
    ->condition('pid_id', $access_log_id)
    ->condition('time', array($ago, time()), 'BETWEEN')
    ->execute();
  $views_data = array();
  foreach ($views_result as $row) {
    $label = date("Y-m", $row->time);
    if (array_key_exists($label, $views_data)) {
      $views_data[$label]++;
    }
    else {
      $views_data[$label] = 1;
    }
  }

  // Define an alter hook to allow third-party modules to add data from other
  // sources (e.g. gathered outside of Islandora) and merge them with $output.
  $context = array('pid' => $pid, 'type' => 'views');
  drupal_alter('islandora_usage_stats_charts_usage', $views_data, $context);
  ksort($views_data, SORT_NATURAL);

  // To make sure we display the correct number of months.
  $views_data = array_slice($views_data, -$num_months);

  // Get the downloads stats going back X months and group them by month.
  $downloads_chart_values = array();
  $placeholders = array(
    ':ago' => $ago,
    ':now' => time(),
    ':pid_id' => $access_log_id,
  );
  if (variable_get('islandora_usage_stats_charts_show_datastreams_downloads', 1)) {
    $dsids_to_exclude = variable_get('islandora_usage_stats_charts_dsids_to_skip', 'RELS-EXT,RELS-INT,TECHMD,MODS,DC');

    if (strlen($dsids_to_exclude)) {
      $dsids_to_exclude_array = explode(',', $dsids_to_exclude);
      $placeholders[':excluded_dsids'] = $dsids_to_exclude_array;
      $downloads_result = db_query("SELECT dsid,time FROM {islandora_usage_stats_datastreams}, {islandora_usage_stats_object_ds_access_log} WHERE {islandora_usage_stats_object_ds_access_log}.time BETWEEN :ago AND :now AND {islandora_usage_stats_datastreams}.pid_id = :pid_id AND {islandora_usage_stats_datastreams}.id = {islandora_usage_stats_object_ds_access_log}.ds_id AND dsid NOT IN (:excluded_dsids)", $placeholders);
    }
    else {
      $downloads_result = db_query("SELECT time FROM {islandora_usage_stats_datastreams}, {islandora_usage_stats_object_ds_access_log} WHERE {islandora_usage_stats_object_ds_access_log}.time BETWEEN :ago AND :now AND {islandora_usage_stats_datastreams}.pid_id = :pid_id AND {islandora_usage_stats_datastreams}.id = {islandora_usage_stats_object_ds_access_log}.ds_id", $placeholders);
    }

    $downloads_data = array();
    foreach ($downloads_result as $row) {
      $label = date("Y-m", $row->time);
      if (array_key_exists($label, $downloads_data)) {
        $downloads_data[$label]++;
      }
      else {
        $downloads_data[$label] = 1;
      }
    }

    // Define an alter hook to allow third-party modules to add data from other
    // sources (e.g. gathered outside of Islandora) and merge them with $output.
    $context = array('pid' => $pid, 'type' => 'downloads');
    drupal_alter('islandora_usage_stats_charts_usage', $downloads_data, $context);
    ksort($downloads_data, SORT_NATURAL);

    // To make sure we display the correct number of months.
    $downloads_data = array_slice($downloads_data, -$num_months);
  }

  // First, assemble the labels from the keys of the views and downloads arrays.
  // If downloads are turned off, just use the keys from the views data.
  if (variable_get('islandora_usage_stats_charts_show_datastreams_downloads', 1)) {
    $chart_labels = array_merge(array_keys($views_data), array_keys($downloads_data));
    $chart_labels = array_unique($chart_labels);
    sort($chart_labels, SORT_NATURAL);
  }
  else {
    $chart_labels = array_keys($views_data);
    sort($chart_labels, SORT_NATURAL);
  }

  // Then, create two arrays for the values, paralleling the list of month keys,
  // to pass into Chart.js. If downloads are turned off, just use the values
  // from the views data.
  if (variable_get('islandora_usage_stats_charts_show_datastreams_downloads', 1)) {
    $views_chart_values = array();
    foreach ($chart_labels as $label) {
      if (array_key_exists($label, $views_data)) {
        $views_chart_values[] = $views_data[$label];
      }
      else {
        $views_chart_values[] = 0;
      }
    }

    $download_chart_values = array();

    foreach ($chart_labels as $label) {
      if (array_key_exists($label, $downloads_data)) {
        $downloads_chart_values[] = $downloads_data[$label];
      }
      else {
        $downloads_chart_values[] = 0;
      }
    }
  }
  else {
    $views_chart_values = array_values($views_data);
  }

  drupal_add_js(drupal_get_path('module', 'islandora_usage_stats_charts') . '/js/usageStatsChart.js', array('scope' => 'footer'));

  $config_settings = array(
    'viewsColor' => variable_get('islandora_usage_stats_charts_views_color', '#002db3'),
    'downloadsColor' => variable_get('islandora_usage_stats_charts_downloads_color', '#99b3ff'),
    'chartWidth' => variable_get('islandora_usage_stats_charts_max_chart_width', '50'),
    'limitChartHeight' => variable_get('islandora_usage_stats_charts_limit_chart_height', 1),
    'showDownloads' => variable_get('islandora_usage_stats_charts_show_datastreams_downloads', 0),
    'numMonthsData' => $num_months,
  );
  drupal_add_js(array('islandora_usage_stats_charts' => $config_settings), 'setting');
  drupal_add_js(
    array(
      'islandora_usage_stats_charts' => array(
        'chartLabels' => $chart_labels,
        'viewsChartValues' => $views_chart_values,
        'downloadsChartValues' => $downloads_chart_values,
      )), 'setting');

  drupal_add_css(drupal_get_path('module', 'islandora_usage_stats_charts') . '/css/islandora_usage_stats_charts.css');
  $collapsed = variable_get('islandora_usage_stats_charts_collapsed', 1) ? 'collapsed' : '';

  $csv_link = NULL;
  if (module_exists('islandora_usage_stats_csv') && variable_get('islandora_usage_stats_charts_show_csv_download_link', 1)) {
    $object = islandora_object_load($pid);
    if (islandora_object_access('download islandora usage stats data', $object)) {
      $csv_link = l(t('Download CSV containing full usage stats'), '/islandora/object/' . $pid . '/usage_stats_csv');
    }
  }

  if (empty($views_chart_values)) {
    $content = '<div>' . t("There is no usage data for this item.") . '<div>';
  }
  else {
    $content = '<canvas id="islandora-usage-stats-chart" height="400px" width="400px"></canvas>';
  }

  $markup = theme('islandora_usage_stats_charts', array(
    'collapsed' => $collapsed,
    'content' => $content,
    'csv_link' => $csv_link,
  ));
  return $markup;
}

/**
 * Populate the usage statistics block for collection objects.
 */
function islandora_usage_stats_charts_generate_collection_stats($pid) {
  drupal_add_js('misc/collapse.js');
  drupal_add_js('misc/form.js');
  $collection_access_log_id = islandora_usage_stats_charts_get_object_access_id($pid);

  $js_url = variable_get('islandora_usage_stats_charts_javascript_url', 'https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.7.2/Chart.bundle.js');
  drupal_add_js($js_url, 'external');

  // Get usage stats going back X months and group them by month.
  $num_months = variable_get('islandora_usage_stats_charts_months_to_show', '6');
  $ago = strtotime('-' . $num_months . ' months', time());
  $placeholders = array(
    ':ago' => $ago,
    ':now' => time(),
    ':collection_id' => $collection_access_log_id,
  );
  $result = db_query('SELECT time FROM {islandora_usage_stats_object_access_log}, {islandora_usage_stats_collection_access_log} WHERE {islandora_usage_stats_object_access_log}.time between :ago AND :now AND {islandora_usage_stats_collection_access_log}.collection = :collection_id AND {islandora_usage_stats_collection_access_log}.object_access_id = {islandora_usage_stats_object_access_log}.id', $placeholders);
  $data = array();
  foreach ($result as $row) {
    $label = date("Y-m", $row->time);
    if (array_key_exists($label, $data)) {
      $data[$label]++;
    }
    else {
      $data[$label] = 1;
    }
  }

  // Define an alter hook to allow third-party modules to add data from other
  // sources (e.g. gathered outside of Islandora) and merge them with $output.
  $context = array('pid' => $pid, 'type' => 'collections');
  drupal_alter('islandora_usage_stats_charts_usage', $data, $context);
  ksort($data, SORT_NATURAL);

  // To make sure we display the correct number of months.
  $data = array_slice($data, -$num_months);

  $chart_labels = array_keys($data);
  $views_chart_values = array_values($data);

  drupal_add_js(drupal_get_path('module', 'islandora_usage_stats_charts') . '/js/usageStatsChart.js', array('scope' => 'footer'));

  $config_settings = array(
    'viewsColor' => variable_get('islandora_usage_stats_charts_views_color', '#002db3'),
    'downloadsColor' => variable_get('islandora_usage_stats_charts_downloads_color', '#99b3ff'),
    'chartWidth' => variable_get('islandora_usage_stats_charts_max_chart_width', '50'),
    'limitChartHeight' => variable_get('islandora_usage_stats_charts_limit_chart_height', 1),
    'showDownloads' => 0,
    'numMonthsData' => $num_months,
  );
  drupal_add_js(array('islandora_usage_stats_charts' => $config_settings), 'setting');
  drupal_add_js(
    array(
      'islandora_usage_stats_charts' => array(
        'chartLabels' => $chart_labels,
        'viewsChartValues' => $views_chart_values,
      )), 'setting');

  if (empty($views_chart_values)) {
    $content = '<div>' . t("There is no usage data for this collection.") . '<div>';
  }
  else {
    $content = '<canvas id="islandora-usage-stats-chart" height="400px" width="400px"></canvas>';
  }

  $collapsed = variable_get('islandora_usage_stats_charts_collapsed', 1) ? 'collapsed' : '';
  $markup = theme('islandora_usage_stats_charts', array(
    'collapsed' => $collapsed,
    'content' => $content,
    'csv_link' => NULL,
  ));
  return $markup;
}

/**
 * Get the ID in the object access table corresponding to the incoming PID.
 *
 * @param string $pid
 *   The object's PID.
 *
 * @return string
 *   The corresponding ID from the islandora_usage_stats_objects table.
 */
function islandora_usage_stats_charts_get_object_access_id($pid) {
  $result = db_select('islandora_usage_stats_objects')
    ->fields('islandora_usage_stats_objects', array('id'))
    ->condition('pid', $pid)
    ->execute();
  return $result->fetchAssoc();
}

/**
 * Query the resource index to get a list of all content model objects.
 *
 * @return array
 *   Associative array of pid => label pairs.
 */
function islandora_usage_stats_charts_get_cmodels() {
  $skip_these = array(
    'fedora-system:FedoraObject-3.0',
    'fedora-system:ServiceDefinition-3.0',
    'fedora-system:ContentModel-3.0',
    'fedora-system:ServiceDeployment-3.0',
  );

  // Query the rindex to get all cmodels.
  $tuque = islandora_get_tuque_connection();
  $ri_query = 'PREFIX fedora-model: <info:fedora/fedora-system:def/model#>
select $object $label from <#ri>
     where { $object fedora-model:hasModel <info:fedora/fedora-system:ContentModel-3.0> ;
      fedora-model:label $label .}';
  $results = $tuque->repository->ri->sparqlQuery($ri_query, 'unlimited');
  $cmodels = array();
  foreach ($results as $member) {
    if (!in_array($member['object']['value'], $skip_these)) {
      $pid = preg_replace('/info:fedora\//', '', $member['object']['value']);
      $label = $member['label']['value'];
      $cmodels[$pid] = $label;
    }
  }
  return $cmodels;
}

/**
 * Checks to see if the current object has any of the allowed content models.
 *
 * @return array
 *   An array containing the content models that the object has that
 *   are allowed.
 */
function islandora_usage_stats_charts_check_for_cmodels($object) {
  $object_models = $object->models;
  $allowed_models = variable_get('islandora_usage_stats_charts_allowed_cmodels', array());
  $allowed_models_filtered = array();
  foreach ($allowed_models as $key => $value) {
    if ($value != '0') {
      $allowed_models_filtered[] = $value;
    }
  }
  return array_intersect($object_models, $allowed_models_filtered);
}
