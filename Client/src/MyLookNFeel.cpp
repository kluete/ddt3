// custom look & feel

#include <cassert>
#include <memory>

#include "lx/juce/jeometry.h"
#include "lx/color.h"

#include "JuceHeader.h"

#include "Controller.h"

#include "logImp.h"

#include "UIConst.h"

using namespace	std;
using namespace juce;
using namespace LX;
using namespace DDT_CLIENT;

//---- Look & Feel overload ---------------------------------------------------

class MyLookAndFeel : public LookAndFeel_V3
{
public:
	MyLookAndFeel(Controller &controller)
		: m_Controller(controller)
	{
		(void)m_Controller;
		
		// setColour(Label::textColourId, Colours::white);
		// setColour(ToggleButton::textColourId, Colours::white);
		
		// disable focus rect around radio buttons
		setColour(TextEditor::focusedOutlineColourId, Colours::transparentBlack);
	}
	
	virtual ~MyLookAndFeel()
	{
		uLog(DTOR, "MyLookAndFeel::DTOR");
	}
	
// overloads
	
	void	drawTableHeaderBackground(Graphics &g, TableHeaderComponent &header) override
	{
		const iRect	r(header.getLocalBounds());
		
		const Rectangle<float>	rf(r.toFloat());
		
		ColourGradient	v_grad(		Color(RGB_COLOR::BAR_GRADIENT_TOP),
						0.f, 0.f,
						Color(RGB_COLOR::BAR_GRADIENT_BOTTOM),
						0.f, r.h(),
						false);		// radial?
		
		g.setGradientFill(v_grad);
		g.fillRect(rf);
		
		const int	THICK = 1;
		
		// vertical separators
		for (int i = 0; i < header.getNumColumns(true); i++)
		{ 	
			const iRect	col_r(header.getColumnPosition(i));
			const iRect	inset_r(col_r.Inset(1));
			
			// north-west bevel (highlight)
			g.setColour(Color(RGB_COLOR::BAR_BEVEL_NORTH_WEST));
			// west
			g.fillRect(inset_r.x(), inset_r.bottom(), THICK, inset_r.h());
			// north
			g.fillRect(inset_r.x(), inset_r.y(), inset_r.w(), THICK);
			
			// south-east bevel
			g.setColour(Color(RGB_COLOR::BAR_BEVEL_SOUTH_EAST));		// inner rect is ONE MORE INSIDE
			// south
			g.fillRect(inset_r.x(), inset_r.bottom(), inset_r.w(), THICK);
			// east
			g.fillRect(inset_r.right(), inset_r.top(), THICK, inset_r.h());
			
			// outline
			g.setColour(Color(RGB_COLOR::BAR_OUTLINE));
			g.drawRect(col_r, THICK);
		}
	}
	
	#if 0
	void	drawTableHeaderColumn(Graphics &g, const String &columnName, int columnId, int w, int h, bool isMouseOver, bool isMouseDown, int columnFlags) override
	{
		if (isMouseDown)	g.fillAll(Colour(0x8899aadd));		// what are these colors???
		else if (isMouseOver)	g.fillAll(Colour(0x5599aadd));
		
		const bool	right_align_f = Track::IsRightAlignCol((TRACK_COL) columnId);

		iRect	area(w, h);
		area.reduce(4, 0);

		if (columnFlags & (TableHeaderComponent::sortedForwards | TableHeaderComponent::sortedBackwards))
		{	// sort icon
			Path	sortArrow;
			
			const bool	up_f = (columnFlags & TableHeaderComponent::sortedForwards);
			
			sortArrow.addTriangle(0.0f, 0.0f, 0.5f, up_f ? -0.8f : 0.8f, 1.0f, 0.0f);

			g.setColour(Colours::white);
			g.fillPath(sortArrow, sortArrow.getTransformToScaleToFit(area.removeFromRight(h / 2).reduced(2).toFloat(), true));
		}
		
		// area.reduce(2, 0);		// reduces TOO MUCH?
		
		g.setColour(Colours::white);
		g.setFont(Font(h * 0.7f, Font::plain));
		g.drawFittedText(columnName, area, right_align_f ? Justification::centredRight : Justification::centredLeft, 1/*max lines*/);
		// g.drawText(columnName, area, right_align_f ? Justification::centredRight : Justification::centredLeft, true/*ellipsize*/);
	}
	#endif

private:
	
	Controller	&m_Controller;
};

//---- INSTANTIATE ------------------------------------------------------------

LookAndFeel*	Controller::CreateLookNFeel(void)
{
	auto	lnf = new MyLookAndFeel(*this);
	assert(lnf);
	
	LookAndFeel::setDefaultLookAndFeel(lnf);
	
	return lnf;
}

// nada mas

