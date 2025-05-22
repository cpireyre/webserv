<?php
  if (isset($_POST['name']))
    setcookie("name",$_POST['name'], time()+3600, "/cgi/", "127.0.0.1", true);
  $isNamed = isset($_COOKIE['name']);
  if($isNamed) : ?>
    <p>Hi, <?php echo $_COOKIE['name']; ?>. Wanna change your name?</p>
  <?php else : ?><p>I don't know you. What's your name?</p>
  <?php endif; ?>

<form method="POST" action="/cgi/cookie.php">
  <input type="text" name="name"/>
  <input type="submit">
</form>
