<?php

/**
 * @file
 * Search API data alteration callback
 */
class SearchApiFilesAlterSettings extends SearchApiAbstractAlterCallback {
  /**
   * Alter items
   *
   * @param array $items
   */
  public function alterItems(array &$items) {

    $files = array();

    if ($this->index->item_type == 'file') {
      foreach ($items as &$item) {
        /*$file = array();
        foreach ($item as $key => $value) {
          $file[$key] = $value;
        }*/

        $files[] = (array) $item;
      }
    }
    else {
      $fields = self::getFileFields();

      foreach ($items as $id => &$item) {
        foreach ($fields as $name => $field) {
          if (isset($item->$name)) {
            foreach ($item->$name as $value) {
              foreach ($value as $file) {
                $files[] = $file;
              }
            }
          }
        }
      }
    }

    $files = $this->getMaxFiles($files);

    foreach ($files as $file) {
      if (self::canScanFile($file) && !self::isTemporary($file)) {
        $attachments = 'attachments_' . $name;
        $item->$attachments .= self::pdfContent($file) . ' ';
      }
    }

  }

  /**
   * Filter items in order to max the size of the processed content to X
   *
   * @param $files
   * @return array
   */
  public function getMaxFiles($files) {
    ini_set("memory_limit", "256M");
    $filessizes = 0;
    $outFiles = array();

    foreach ($files as $file) {
      if ($filessizes + $file['filesize'] < 60 * 1024 * 1024) {
        $filessizes += $file['filesize'];
        $outFiles[] = $file;
      }
    }

    return $outFiles;
  }

  /**
   * Configuration form
   * @return mixed
   */
  public function configurationForm() {
    $default = search_api_files_default_files_ext();

    $form['include_extensions'] = array(
      '#type' => 'checkboxes',
      '#title' => t('File type to scan'),
      '#options' => $default,
      '#default_value' => isset($this->options['include_extensions']) ? $this->options['include_extensions'] : $default,
      '#size' => 80,
      '#maxlength' => 255,
      #'#description' => t('File extensions that are excluded from indexing. Separate extensions with a space and do not include the leading dot. Extensions are internally mapped to a MIME type, so it is not necessary to put variations that map to the same type (e.g. tif is sufficient for tif and tiff)'),
    );
    return $form;
  }

  /**
   * @return array
   */
  public function propertyInfo() {
    $ret = array();
    if ($this->index->item_type == 'file') {
      $ret['attachments_content'] = array(
        'label' => 'File content',
        'description' => 'File content',
        'type' => 'text',
      );
    }
    else {
      $fields = self::getFileFields();
      foreach ($fields as $name => $field) {
        $ret['attachments_' . $name] = array(
          'label' => 'File field content: ' . $name,
          'description' => $name,
          'type' => 'text',
        );
      }
    }
    return $ret;
  }

  /**
   * @return array
   */
  protected function getFileFields() {
    $ret = array();
    foreach (field_info_fields() as $name => $field) {
      if ($field['type'] == 'file' && array_key_exists($this->index->item_type, $field['bundles'])) {
        $ret[$name] = $field;
      }
    }
    return $ret;
  }

  /**
   * @param $file
   * @return string
   */
  protected function getFileContent($file) {
    switch (TRUE) {
      case (stripos($file['filemime'], 'pdf') !== FALSE):
        return self::pdfContent($file);
        break;
    }
  }

  /**
   * @param $file
   * @return string
   */
  protected function pdfContent($file) {

    $text = '';

    $filepath = drupal_realpath($file['uri']);

    require_once drupal_get_path('module', 'search_api_files') . '/vendor/autoload.php';

    try {
      $parser = new Smalot\PdfParser\Parser();

      $pdf = $parser->parseFile($filepath);

      $text = $pdf->getText();

    }
    catch (Exception $e) {
      watchdog('search_api_file_exception', 'Unable to open file "@uri": @message
      Real Path: @filepath', array(
        '@uri' => $file['uri'],
        '@message' => $e->getMessage(),
        '@filepath' => $filepath,
      ));
    }


    return $text;

  }

  /**
   * Test if the file is a pdf
   * @param array $file
   * @return bool
   */
  private function canScanFile(array $file) {
    foreach ($this->options['include_extensions'] as $type) {
      if (stripos($file['filemime'], $type) !== FALSE) {
        return TRUE;
      }
    }
    return FALSE;
  }


  /**
   * @param $file
   * @return bool
   */
  protected function isTemporary($file) {
    return (strpos($file['uri'], 'temporary://') !== FALSE);
  }

}
