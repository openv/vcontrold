#ifndef VERSION

/*
 * TODO: reliable versioning scheme
 */

#ifdef GIT_CODE_VERSION
#define VERSION GIT_CODE_VERSION
#else
/*
 * Legacy code version
 */
#define VERSION "v0.99.3-legacy"
#endif /* GIT_CODE_VERSION */

#endif /* VERSION */


