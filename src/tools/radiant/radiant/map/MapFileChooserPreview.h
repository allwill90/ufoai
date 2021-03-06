#pragma once

#include "../ui/common/MapPreview.h"
#include "gtkutil/filechooser.h"
#include "ireference.h"

namespace map
{

	class MapFileChooserPreview: public gtkutil::FileChooser::Preview
	{
			// The loaded map resource
			Resource* _mapResource;

			// The name of the map being previewed
			std::string _mapName;

			GtkWidget* _previewContainer;

			// The actual MapPreview widget
			ui::MapPreview _preview;

		public:
			MapFileChooserPreview ();

			virtual ~MapFileChooserPreview ();

			// Retrieve the preview widget for packing into the dialog
			GtkWidget* getPreviewWidget ();

			/**
			 * Gets called whenever the user changes the file selection.
			 * Note: this method must call the setPreviewActive() method on the
			 * FileChooser class to indicate whether the widget is active or not.
			 */
			void onFileSelectionChanged (const std::string& newFileName, gtkutil::FileChooser& fileChooser);

		private:

			// Sets the name of the map to preview, returns TRUE on success
			bool setMapName (const std::string& name);
	};

} // namespace map
