class TextBox { //creates a textbox
  float modifier; //to fix the length based on the text
  float pointer;
  //accepts textsize as a font size, and a text limit being the number of characters
  //indent and margin set the offset for fine adjusting appearances
  
  void createTextBox(float x, float y, int textSize,int textLimit) { //creates the textbox, basically a white rectangle
    fill(255);
    modifier = textSize/1.3*textLimit;
    rect(x,y,modifier,textSize,2); //changes size based on the text
    
  }
  
  void drawIndicator(float x, float y, int textSize) { //draws the little line for the text
    fill(178, 220, 252); //slight blue color
    rect(x, y, modifier+10, textSize+10, 2);
  }
  
  void writeText(float indent, float margin, int textSize, String text, color textColor) { //inserts the text into the box
    fill(textColor);
    textSize(textSize);
    text(text, indent, margin);
  }
  
  String modify(int textLimit, String text) { //allows the textbox to be typed into
    if (int(key) == 65535 || key == BACKSPACE) { //if backspacing
      if (text.length() != 0) text = text.substring(0, text.length()-1); //stop at last character
    }else if (key != CODED && key != ENTER) { //if a letter key
      text += key; //add the text
      if (text.length() >= textLimit) text = text.substring(0,textLimit); //limit length
    }
    return text; //returns the text
  }
}
