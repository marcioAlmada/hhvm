<?php

$test_run_id = rand(100000, 999999);
$error_log_file = fopen("${LOG_ROOT}_test$test_run_id.log", 'w');

function tlog($str) {
  global $error_log_file;

  fwrite($error_log_file, $str);
  fwrite($error_log_file, "\n");
  // error_log($str);
}

function dumpLogFilesToStdoutAndDie() {
  global $error_log_file;
  global $test_run_id;
  global $LOG_ROOT;

  sleep(1);
  error_log('-------------------------------------------');
  error_log("Contents of '${LOG_ROOT}_test$test_run_id.log'");
  readfile("${LOG_ROOT}_test$test_run_id.log");
  echo "\n";
  error_log('-------------------------------------------');
  error_log("Contents of '${LOG_ROOT}_test_server$test_run_id.log'");
  readfile("${LOG_ROOT}_test_server$test_run_id.log");
  echo "\n";
  error_log('-------------------------------------------');
  error_log("Contents of '${LOG_ROOT}_test_server_stdout$test_run_id.log'");
  readfile("${LOG_ROOT}_test_server_stdout$test_run_id.log");
  echo "\n";
  error_log('-------------------------------------------');
  error_log("Contents of '${LOG_ROOT}_test_server_stderr$test_run_id.log'");
  readfile("${LOG_ROOT}_test_server_stderr$test_run_id.log");
  echo "\n";
  error_log('-------------------------------------------');
  error_log("Contents of '${LOG_ROOT}_test_client$test_run_id.log'");
  readfile("${LOG_ROOT}_test_client$test_run_id.log");
  echo "\n";
  error_log('-------------------------------------------');
  error_log("Contents of '${LOG_ROOT}_test_sandbox_access.log'");
  readfile("${LOG_ROOT}_test_sandbox_access.log");
  echo "\n";
  error_log('-------------------------------------------');
  error_log("Contents of '${LOG_ROOT}_curl$test_run_id.log'");
  readfile("${LOG_ROOT}_curl$test_run_id.log");
  echo "\n";
  error_log('-------------------------------------------');
  throw new Exception("test failed");
}

function hphp_home() {
  // __DIR__ == result.'hphp/test/server/util'
  return realpath(__DIR__.'/../../../..');
}

function bin_root() {
  $dir = hphp_home() . '/' . '_bin';
  return is_dir($dir) ?
    $dir :      # fbmake
    hphp_home() # github
  ;
}

function get_random_port() {
  $BasePort = 20000;
  $PortRange = 3000;
  return rand($BasePort, $BasePort+$PortRange);
}

function startServer($serverPort, $adminPort, $debugPort, $home, $root) {
  global $test_run_id, $LOG_ROOT;

  $portConfig = ' -vServer.Port='.$serverPort;
  $serverConfig = ' --config='.$home.'/config/server.hdf';
  $logFileConfig = ' -vLog.File='."${LOG_ROOT}_test_server$test_run_id.log";
  $srcRootConfig = ' -vServer.SourceRoot='.$root;
  $includePathConfig = ' -vServer.IncludeSearchPaths.0='.$root;
  $adminPortConfig = ' -vAdminServer.Port='.$adminPort;
  $debugPortConfig = $debugPort ? ' -vEval.Debugger.Port='.$debugPort : '';
  $repoConfig = " -vRepo.Central.Path=${LOG_ROOT}_server$test_run_id.hhbc";
  $useJit = array_key_exists('HHVM_JIT', $_ENV) && $_ENV['HHVM_JIT'] == 1;
  $jitConfig = ' -vEval.Jit='.($useJit ? "true" : "false");
  // To emulate sandbox setup, let Sandbox.Home be '$home'
  // and user name be 'debugger', so that the server can find the
  // sandbox_conf.hdf in '$root'.
  $sandboxHomeConfig = ' -vSandbox.Home='.$home;

  $hhvm = PHP_BINARY;

  if (ini_get('hhvm.repo.authoritative')) {
    $cmd = "$hhvm --hphp -k1 -l2 -t hhbc -o ${LOG_ROOT}_bytecode$test_run_id ".
      "--input-dir $root";
    tlog('Building repo with command: '.$cmd);
    tlog(exec($cmd));
    $repoConfig .=
      " -vRepo.Local.Path=${LOG_ROOT}_bytecode$test_run_id/hhvm.hhbc".
      " -vRepo.Authoritative=true";
  }

  $cmd = $hhvm.' --mode=server' . $serverConfig . $logFileConfig .
    ' -vEval.JitProfileInterpRequests=0' . $portConfig . $srcRootConfig .
    $includePathConfig . $sandboxHomeConfig . $adminPortConfig .
    $debugPortConfig . $repoConfig . $jitConfig .
     " > ${LOG_ROOT}_test_server_stdout$test_run_id.log" .
     " 2> ${LOG_ROOT}_test_server_stderr$test_run_id.log";

  tlog('Starting server with command: '.$cmd);
  $pipes = array();
  $serverProc = proc_open($cmd, array(), $pipes);
  if (!is_resource($serverProc)) {
    tlog('Failed to start a shell process for the server');
    dumpLogFilesToStdoutAndDie();
  }
  return $serverProc;
}

function waitForServerToGetGoing($serverPort) {
  $host = php_uname('n');
  $r = "";
  for ($i = 1; $i <= 20; $i++) {
    sleep(1);
    $r = request($host, $serverPort, "hello.php");
    if ($r === "Hello, World!") return;
    tlog('Server replied: '.$r);
  }

  tlog('Server is not responding.');
  dumpLogFilesToStdoutAndDie();
}

function stopServer($adminPort) {
  global $test_run_id, $LOG_ROOT;

  $r = "";
  for ($i = 1; $i <= 10; $i++) {
    $r = request(php_uname('n'), $adminPort, 'stop');
    if ($r == "OK") break;
    usleep(100000);
  }
  if ($r != "OK") {
    tlog("Server did not stop. Response was $r");
    dumpLogFilesToStdoutAndDie();
  }
  unlink("${LOG_ROOT}_test$test_run_id.log");
  unlink("${LOG_ROOT}_test_server$test_run_id.log");
  unlink("${LOG_ROOT}_test_server_stderr$test_run_id.log");
  unlink("${LOG_ROOT}_test_server_stdout$test_run_id.log");
  unlink("${LOG_ROOT}_test_client$test_run_id.log");
  unlink("${LOG_ROOT}_server$test_run_id.hhbc");
  unlink("${LOG_ROOT}_client$test_run_id.hhbc");
  unlink("${LOG_ROOT}_curl$test_run_id.log");
  unlink("${LOG_ROOT}_bytecode$test_run_id/hhvm.hhbc");
  rmdir("${LOG_ROOT}_bytecode$test_run_id");
}

function http_request($host, $port, $path, $timeout = 1200) {
  global $test_run_id, $LOG_ROOT;

  $url = "http://$host:$port/$path";
  $host_name = "hphpd.debugger.".php_uname('n');
  $cmd = "curl --trace-ascii ${LOG_ROOT}_curl$test_run_id.log ".
    "--silent --connect-timeout $timeout ".
    "--header 'Host: $host_name' --url $url";
  tlog("Requesting page with command: $cmd");
  return exec($cmd);
}

if (!function_exists("request")) {
  function request($host, $port, $path, $timeout = 1200) {
    return http_request($host, $port, $path, $timeout);
  }
}

function killChildren($pid) {
  $childIds = exec("pgrep -f -d , -P $pid");
  foreach (explode(",", $childIds) as $cid) {
    if (!$cid) continue;
    tlog("killing ".exec("ps -f -p ".$cid));
    killChildren($cid);
    posix_kill($cid, SIGKILL);
  }
}
