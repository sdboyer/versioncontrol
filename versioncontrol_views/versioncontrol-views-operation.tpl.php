<?php
// $Id$
?>
<div class="operation">
  <div class="title"><?php print $title; ?></div>
  <?php  if (!empty($description)): ?>
    <div class="description"><pre><?php print $description; ?></pre></div>
  <?php endif; ?>
  <div class="items"><?php print $items ?></div>
</div>
