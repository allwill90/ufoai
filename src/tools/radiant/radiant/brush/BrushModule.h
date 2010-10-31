/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(INCLUDED_BRUSHMODULE_H)
#define INCLUDED_BRUSHMODULE_H

#include "iregistry.h"

#include "TexDef.h"
#include "ibrush.h"

#include "preferencesystem.h"


// Some constants
namespace {
const std::string RKEY_ENABLE_TEXTURE_LOCK = "user/ui/brush/textureLock";
}

class BrushModuleClass: public RegistryKeyObserver, public BrushCreator, public PreferenceConstructor
{
		bool _textureLockEnabled;

	public:
		// Constructor
		BrushModuleClass ();

		// This constructs the brush preferences, initialises static variables, etc.
		void construct ();

		// The opposite of the above construct()
		void destroy ();

		// --------------- BrushCreator methods ---------------------------------------------

		// Creates a new brush node on the heap and returns it
		scene::Node& createBrush ();

		void Brush_forEachFace (scene::Node& brush, const BrushFaceDataCallback& callback);

		// Adds a face plan to the given brush
		bool Brush_addFace (scene::Node& brush, const _QERFaceData& faceData);

		// ----------------------------------------------------------------------------------

		// Re-constructs the BrushClipPlane to activate the colour change
		void clipperColourChanged ();

		// returns true if the texture lock is enabled
		bool textureLockEnabled () const;
		void setTextureLock (bool enabled);

		// Switches the texture lock on/off
		void toggleTextureLock ();

		// The callback for registry key changes
		void keyChanged ();

	private:
		// Adds the actual preference items to the given page
		void constructPreferencePage (PreferenceGroup& group);

}; // class BrushModuleClass

// The accessor function declaration
BrushModuleClass* GlobalBrush ();

#endif
