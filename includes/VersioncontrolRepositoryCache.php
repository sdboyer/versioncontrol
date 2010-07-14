<?php

/**
 * @file
 * Needed basic clases that are not entities.
 */

/**
 * Repository loader, singleton class.
 */
final class VersioncontrolRepositoryCache {
  private static $instance;
  private $backends = array();
  private $repoCache = array('repo_id' => array(), 'name' => array(), 'vcs' => array());
  /**
   * Internal state variable indicating whether or not all repositories have
   * been fetched (via VersioncontrolRepositoryCache::getInstance()->getAllRepositories()).
   * @var bool
   */
  private $allFetched = FALSE;

  private function __construct() {
    // TODO really oughtta make this better.
    $this->backends = versioncontrol_get_backends();
    $result = db_query('SELECT repo_id, name, vcs FROM {versioncontrol_repositories}');
    // Cache a skeletal, low-mem overhead list of all the repos we have.
    while ($item = db_fetch_object($result)) {
      $this->repoCache['repo_id'][$item->repo_id] = &$item;
      // $this->repoCache['name'][$item->name] = &$item;
      // $this->repoCache['vcs'][$item->vcs] = &$item;
    }

    $this->repoCache['repo_ids'] = &$this->repoCache['repo_id']; // backwards compat
  }

  /**
   * Return the singleton's instance of the VersioncontrolRepositoryCache.
   *
   * @return VersioncontrolRepositoryCache
   */
  public static function getInstance() {
    if (!self::$instance instanceof VersioncontrolRepositoryCache) {
      self::$instance = new VersioncontrolRepositoryCache();
    }
    return self::$instance;
  }

  /**
   * Convenience function for retrieving one single repository by repository id
   * from cache.
   *
   * @return
   *   A single VersioncontroRepository array.
   *   If no repository corresponds to the given repository id, NULL is returned.
   */
  public function getRepository($repo_id) {
    if (!isset($this->repoCache['repo_id'][$repo_id])) {
      // no such repo. bail out.
      return;
    }
    if (!$this->repoCache['repo_id'][$repo_id] instanceof VersioncontrolRepository) {
      $repos = $this->getRepositories(array('repo_id' => $repo_id));
      $repo = reset($repos);
      $this->cacheRepository($repo);
    }
    return $this->repoCache['repo_id'][$repo_id];
  }

  /**
   * Retrieve a set of repositories that match the given constraints.
   *
   * @static
   * @param $constraints
   *   An optional array of constraints. Possible array elements are:
   *
   *   - 'vcs': An array of strings, like array('cvs', 'svn', 'git').
   *       If given, only repositories for these backends will be returned.
   *   - 'repo_ids': An array of repository ids.
   *       If given, only the corresponding repositories will be returned.
   *   - 'names': An array of repository names, like
   *       array('Drupal CVS', 'Experimental SVN'). If given,
   *       only repositories with these repository names will be returned.
   *
   * @return
   *   An array of repositories where the key of each element is the repository
   *   id. The corresponding value contains a VersioncontrolRepository object.
   *   If not a single repository matches these constraints,
   *   an empty array is returned.
   */
  public final function getRepositories($constraints = array()) {
    $auth_methods = versioncontrol_get_authorization_methods();

    if (isset($constraints['repo_ids'])) {
      $repo_ids = array();
      foreach ($constraints['repo_ids'] as $repo_id) {
        $repo_ids[] = (int) $repo_id;
      }
      $constraints['repo_ids'] = $repo_ids;
    }

    $constraints_serialized = serialize($constraints);
    if (isset($this->repoCache[$constraints_serialized])) {
      return $this->repoCache[$constraints_serialized];
    }

    list($and_constraints, $params) =
      _versioncontrol_construct_repository_constraints($constraints, $this->backends);

    // All the constraints have been gathered, assemble them to a WHERE clause.
    $where = empty($and_constraints) ? '' : ' WHERE '. implode(' AND ', $and_constraints);

    $result = db_query('SELECT * FROM {versioncontrol_repositories} r'. $where, $params);

    // Sort the retrieved repositories by backend.
    $repositories_by_backend = array();

    while ($repository = db_fetch_array($result)) {
      if (!isset($this->backends[$repository['vcs']])) {
        // don't include repositories for which no backend module exists
        continue;
      }

      if (!isset($repositories_by_backend[$repository['vcs']])) {
        $repositories_by_backend[$repository['vcs']] = array();
      }
      $repositories_by_backend[$repository['vcs']][$repository['repo_id']] = $repository;
    }

    // Add the fully assembled repositories to the result array.
    $result_repositories = array();
    foreach ($repositories_by_backend as $vcs => $vcs_repositories) {
      foreach ($vcs_repositories as $repository) {
        $vcs_repository = new $this->backends[$repository['vcs']]->classes['repo']($repository['repo_id'], $repository, FALSE);
        $vcs_repository->backend = $this->backends[$repository['vcs']];
        //TODO think how to improve this getter per repo, because it can cause
        //     performance problems, maybe it's better to use a per backend
        //     process(1-level-up foreach) but now it's not posible, because
        //     we can not inherit VersioncontrolRepositoryCache
        $vcs_repository->_getRepository();
        if (!isset($this->repoCache['repo_id'][$repository['repo_id']]) || !$this->repoCache['repo_id'][$repository['repo_id']] instanceof VersioncontrolRepository) {
          $this->cacheRepository($vcs_repository);
        }
        $result_repositories[$repository['repo_id']] = $vcs_repository;
      }
    }


    $this->repoCache[$constraints_serialized] = $result_repositories; // cache the results
    return $result_repositories;
  }

  public function getAllRepositories() {
    if (!$this->allFetched) {
      $this->allFetched = TRUE;
      $repo_ids = array();
      foreach ($this->repoCache['repo_id'] as $repo_id => $min_repo) {
        if (!$min_repo instanceof VersioncontrolRepository) {
          $repo_ids[] = $repo_id;
        }
      }
      foreach ($this->getRepositories(array('repo_ids' => $repo_ids)) as $repo) {
        $this->cacheRepository($repo);
      }
    }
    return $this->repoCache['repo_id'];
  }

  private function cacheRepository(&$repository) {
    $this->repoCache['repo_id'][$repository->repo_id] = &$repository;
    $this->repoCache['name'][$repository->name][$repository->repo_id] = &$repository;
    $this->repoCache['vcs'][$repository->vcs][$repository->repo_id] = &$repository;
  }
}
