/*  Copyright 2007-2017 the original vcontrold development team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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
