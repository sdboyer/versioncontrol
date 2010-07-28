<?php
// $Id$
?>
<div class="versioncontrol-commitlog">
<?php foreach($rows as $group_title => $table): ?>
  <h3><?php print $group_title; ?></h3>
  <div class="versioncontrol-commitlog-messages">
  <?php print $table; ?>
  </div>
<?php endforeach; ?>
</div>
