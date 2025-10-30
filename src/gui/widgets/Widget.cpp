#include "widget.h"

Widget::Widget()
{

}

Widget::Widget(const Widget& other) {
    // Perform deep copy if needed
    showWidget = other.showWidget;
}

// Move constructor
Widget::Widget(Widget&& other) noexcept
{
    showWidget = std::exchange(other.showWidget, false);
}

// Copy assignment operator
Widget& Widget::operator=(const Widget& other) {
    if (this != &other) {
        // Perform deep copy if needed
        showWidget = other.showWidget;
    }
    return *this;
}

// Move assignment operator
Widget& Widget::operator=(Widget&& other) noexcept {
    if (this != &other) {
        showWidget = std::exchange(other.showWidget, false);
    }
    return *this;
}

Widget::~Widget()
{

}

void Widget::render()
{

}
