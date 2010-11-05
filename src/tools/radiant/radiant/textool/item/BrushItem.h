#ifndef BRUSHITEM_H_
#define BRUSHITEM_H_

#include "../../brush/BrushClass.h"
#include "../TexToolItem.h"

namespace selection {
	namespace textool {

class BrushItem :
	public TexToolItem
{
	// The brush this control is referring to
	Brush& _sourceBrush;

public:
	// Constructor, allocates all child FacItems
	BrushItem(Brush& sourceBrush);

	/** greebo: Saves the undoMemento of this brush,
	 * 			so that the operation can be undone later.
	 */
	virtual void beginTransformation();

}; // class BrushItem

	} // namespace TexTool
} // namespace selection

#endif /*BRUSHITEM_H_*/
