class Button {
  float modifier;
  
  boolean checkButton(float x, float y, float buttonSizeX, float buttonSizeY) { //see if button clicked
  //requires the posiiton and size of button
    if (mouseX > x-(buttonSizeX*modifier/2) && mouseX < x+(buttonSizeX*modifier/2) && mouseY > y-(buttonSizeY/2) && mouseY < y+(buttonSizeY/2) && mousePressed) {
      return true; //if mouse is within button and pressed button has been pressed
    }else{
      return false; //ptherwise it hasnt
    }
  }
  //draws the buttons requires the posiiton size, the label and colors of text and button itself
  void drawButton(float x, float y, float buttonSizeX, float buttonSizeY, String buttonLabel, color buttonColor, color textColor) {
    modifier = 0.22*buttonLabel.length(); //scales button based on the label
    if (mouseX > x-(buttonSizeX*modifier/2) && mouseX < x+(buttonSizeX*modifier/2) && mouseY > y-(buttonSizeY/2) && mouseY < y+(buttonSizeY/2)) {
      fill(buttonColor-100); //when moused over it darkens
    }else{
      fill(buttonColor);
    }
    rect(x,y,buttonSizeX*modifier,buttonSizeY,7); //draws the actual graphics for the button
    textSize(buttonSizeX/2.7);
    fill(textColor);
    text(buttonLabel,x-(buttonSizeX*modifier/2.2),y+(buttonSizeY/4)); //places text appropriately
  }
}
