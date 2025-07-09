/*
**    This software license is not yet defined.
**
*/
#ifdef RGB_LED_PIN
#include "UserLed.hpp"

UserLed* UserLed::instance_ = nullptr;

UserLed::UserLed() : state_(State::CLEAR),
            led_(CRGB::Black),
            semaphore_(xSemaphoreCreateMutex()),
            newStateQueue_(xQueueCreate(10, sizeof(NewState)))
{
    instance_ = this;
}

void UserLed::begin(){
    FastLED.addLeds<WS2812B, RGB_LED_PIN, GRB>(&led_, 1);
    FastLED.setBrightness(128);
    FastLED.clear();
}

void UserLed::loop(){
    NewState newState;
    if(xQueueReceive( newStateQueue_, &newState, 0) == pdPASS ){
        xSemaphoreTake(semaphore_, portMAX_DELAY);
        state_ = newState.state;
        xSemaphoreGive(semaphore_);

        switch(newState.state){
            case State::CLEAR:
                if(led_ != CRGB::Black){
                    FastLED.showColor(CRGB::Black);
                }
                break;
            case State::ERROR:
                led_ = CRGB::Red;
                FastLED.show();
                break;
            case State::WARNING:
                led_ = CRGB::Orange;
                FastLED.show();
                break;
            case State::CUSTOM:
                led_ = newState.payload;
                FastLED.show();
                break;
        }

    }else if(state_ == State::ERROR){
        if(led_ == CRGB::Red){
            led_ = CRGB::Black;
            FastLED.show();
        }else{
            led_ = CRGB::Red;
            FastLED.show();
        }
    }else if(state_ == State::WARNING){
        if(led_ == CRGB::Orange){
            led_ = CRGB::Black;
            FastLED.show();
        }else{
            led_ = CRGB::Orange;
            FastLED.show();
        }
    }
}

void UserLed::error()
{
    //Don't send if already in error
    if(getState() != State::ERROR){
        setNewState(State::ERROR);
    }
}

void UserLed::warning()
{
    //Don't send if already in warning
    if(getState() != State::WARNING){
        setNewState(State::WARNING);
    }
}
void UserLed::clearError()
{
    //Don't send if no errors
    if((getState() == State::ERROR) || (getState() == State::WARNING)){
        setNewState(State::CLEAR);
    }
}

void UserLed::setNewState(State newState, uint32_t payload)
{
    NewState state = {newState, payload};
    xQueueSend( /* The handle of the queue. */
            newStateQueue_,
            ( void * ) &state,
            ( TickType_t ) 0 );
}

UserLed::State UserLed::getState()
{
    State ret;
    xSemaphoreTake(semaphore_, portMAX_DELAY);
    ret = state_;
    xSemaphoreGive(semaphore_);
    return ret;
}

void UserLed::customColor(uint8_t r, uint8_t g, uint8_t b)
{
    CRGB customColor;
    customColor.r = r;
    customColor.g = g;
    customColor.b = b;
    setNewState(State::CUSTOM, (uint32_t)customColor);
}

void UserLed::customHSV(float hue, float saturation, float value)
{
    uint8_t r,g,b;
    HSVToRGB(hue, saturation, value, r, g, b);
    customColor(r, g, b);
}

void UserLed::HSVToRGB(float hue, float saturation, float value, uint8_t& red, uint8_t& green, uint8_t& blue)
{
    float r = 0, g = 0, b = 0;
	if (saturation == 0)
	{
		r = value;
		g = value;
		b = value;
	}
	else
	{
		int i;
		double f, p, q, t;

		if (hue == 360)
			hue = 0;
		else
			hue = hue / 60;

		i = (int)trunc(hue);
		f = hue - i;

		p = value * (1.0 - saturation);
		q = value * (1.0 - (saturation * f));
		t = value * (1.0 - (saturation * (1.0 - f)));

		switch (i)
		{
		case 0:
			r = value;
			g = t;
			b = p;
			break;

		case 1:
			r = q;
			g = value;
			b = p;
			break;

		case 2:
			r = p;
			g = value;
			b = t;
			break;

		case 3:
			r = p;
			g = q;
			b = value;
			break;

		case 4:
			r = t;
			g = p;
			b = value;
			break;

		default:
			r = value;
			g = p;
			b = q;
			break;
		}

	}

	red = (uint8_t)(r * 255);
	green = (uint8_t)(g * 255);
	blue = (uint8_t)(b * 255);
}
#endif
