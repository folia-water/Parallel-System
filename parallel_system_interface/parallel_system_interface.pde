import java.util.Calendar; //for calendar and dates
Calendar calendar; //start instance
PFont font; //for fonts

final int buttonSizeX = 200; //values for buttons
final int buttonSizeY = 100;
final int barSizeX = 120;
final int barSizeY = 30;

final color buttonColor = color(92, 108, 240); //some color definitions
final color buttonColorGreen = color(2, 247, 105);
final color buttonColorRed = color(235, 50, 50);
final color buttonTextColor = color(233, 250, 8);
final color textColor = color(0, 0, 0); //black text
final color backgroundColor = color(50, 160, 205); //background blue
final color barColor = color(255, 255, 255); //progress bars are white
final color fillColor = color(0, 255, 0); //fills with green
final int indent = 125; //values for margin and indent spacing
final int margin = 96;
final int textSize = 48;

int time; //for button debounce
int mode; //which screen are we in
final int ERROR = -1; //errors
final int HOME = 0; //defs for different screens
final int SELECT = 1; //for selecting modes
final int CHALLENGE = 2; //the 1 liter run
final int WATER = 3; //the 4 liter run
final int PURGE = 4; //clearing the lines
final int CHALLENGERUN = 5; //are we running the challenge
final int WATERRUN = 6; //are we running the water thru
final int CHECKCANCEL = 7; //for checking if want to go back
final int DATA = 8; //for seeing how the run is going
final int SETTINGS = 9; //for changing settings
int currentMode; //for going back
int purgeTimer; //for keeping track of how long purge is on
final int purgeTimeLimit = 5; //amount of minutes the purge can run continuously
boolean purging = false; //are we runnign a purge?
boolean paused = false; //is a run paused right now?
boolean shouldResume = false; //shoudl we resume a run?

final int numFunnelsTotal = 8; //max for the setup
int numFunnels = 8; //how many funnels running
boolean funnelsSelected = false;
float[] influentVols = new float[numFunnelsTotal+1];
final int firstSamplePoint = 550; //for the sample point of the 1 L run
boolean[] sample = new boolean[numFunnelsTotal+1]; //are we sampling
boolean [] hasTaken = new boolean[numFunnels+1]; //did we take the sample yet?
boolean[] flasher = new boolean[numFunnelsTotal+1]; //for flashing buttons
boolean[] finished = new boolean[numFunnelsTotal+1];
int funnelPointer = 0;
boolean lock = false;

import processing.serial.*; //for talking with the arduino
Serial myPort;
String portName; //for storing the port the arduino is connected to
boolean SDfailed = false; //did the program fail?
boolean connected = false; //is the arduino connected?
boolean runStart = false; //did the run begin yet?
String data; //for storing incoming data
String[] values = new String[numFunnelsTotal+2]; //array to hold each value
String runName; //stores the name of the run

Table table; //for storing the influent volume data
Table pauses; //for storing paused runs
final int saveTimeInterval = 2*60; //save the data to the table at this interval (in s)

import grafica.*; //for graphing stuff
int panel; //in data mode which funnel are we viewing?
int maxPanels; //limit to how many panels
final int maxGraphSize = 1000; //1000 5 min intervals surely wont happen
int dataPointCounter; //for creating point arrays for the graphs

//this utilizes the grafica library for making graphs 
GPointsArray[] points = new GPointsArray[numFunnelsTotal+1]; //create multiple instances of arrays
GPlot[] plot = new GPlot[numFunnelsTotal+1];

// This code requires the Java mail library
// smtp.jar, pop3.jar, mailapi.jar, imap.jar, activation.jar
// Download:// <a href="http://java.sun.com/products/javamail/" target="_blank" rel="nofollow">http://java.sun.com/products/javamail/</a>
 
import javax.mail.*; //for mailing, do sketch add file, javax.mail.jar
import javax.mail.internet.*; //call void send mail to send the email
//arguments of recipeient string, subject string, body string
String reciever = "michael.chan@foliawater.com"; //whos getting these emails
String text = ""; //for saving whats typed in
boolean emailInput; //are we entering an email?
boolean emailInputted; //have we now input an email?
boolean needInput; //do we need to input the email?
boolean textBoxClicked; //did we click the text box
final int textLimit = 25; //max characters can enter into text box
final int runTimeOut = 600; //value after which notifaction is sent that its still running
//600 minutes or 10 hours after which should be done
boolean timeOut; //did we time out?

Button back = new Button(); //declare buttons
Button settings = new Button();
Button start = new Button();
Button stop = new Button();
Button oneLiter = new Button();
Button fourLiter = new Button();
Button up = new Button();
Button down = new Button();
Button next = new Button();
Button purge = new Button();
Button yes = new Button();
Button no = new Button();
Button right = new Button();
Button left = new Button();
Button pause = new Button();
Button resume = new Button();
Button refresh = new Button();
Button email = new Button();
TextBox textBox = new TextBox(); //for entering the email
Button textButton = new Button(); //for clicking on text boxes

Button[] funnel = new Button[numFunnelsTotal+1];

JSONArray emails = new JSONArray(); //JSOn objects for storing emails
JSONObject parse = new JSONObject();
int emailPointer; //for tracking wehre in the emails we are at

void setup() {
  size(displayWidth, displayHeight); //start a screen
  rectMode(CENTER); //drawing rectangles based on center
  font = createFont("Arial", textSize, true); //select our font type
  textFont(font, textSize);
  background(backgroundColor); // background color and clears screen
  calendar = Calendar.getInstance(); //start instance
  mode = HOME; //start on title screen
  for (int i = 1; i <= numFunnelsTotal; i++) { //initialize class instances
    funnel[i] = new Button();
    points[i] = new GPointsArray(maxGraphSize); //create an new array for the graphs
    plot[i] = new GPlot(this); //create plots
  }
  table = new Table(); //for storing the run data
  pauses = new Table(); //for storing pauses
  serialSetup(); //setup serial communication
  if (mode != ERROR) { //if no errors were thrown
    try{ //try to read a data file if its there
      emails = loadJSONArray("emails.json");
      parse = emails.getJSONObject(emails.size()-1);  //get the latest entry
      reciever = parse.getString("email");
      if (reciever == "") throw new Exception("e"); //goto the catch block and retsart
    }catch(Exception e) { //otherwise just print the error
      e.printStackTrace();
      needInput = true; //need to input an email first
    }
  }
}

void serialSetup() { //sets up the serial port to communicate
  printArray(Serial.list()); //start serial communication
  try{ //if the serial port is even conencted or not
    portName = Serial.list()[0];
    myPort = new Serial(this, portName, 115200);
    myPort.bufferUntil(10); //call serial event every new line
  }catch(RuntimeException e) {
    e.printStackTrace();
    mode = ERROR; //error mode
    connected = false; //if system isnt even physically connected
    SDfailed = false; //SD didnt fail bc its not conencted
  }
}

void draw() {
  fill(backgroundColor);
  rect(width/2, height/2, width, height); //clear the previous screen
  switch (mode) { //switch thru the diff mode screens
  case HOME:
    home();
    break;
  case SELECT:
    select();
    break;
  case CHALLENGE:
    challenge();
    break;
  case WATER:
    water();
    break;
  case PURGE:
    purge();
    break;
  case CHALLENGERUN:
    challengeRun();
    break;
  case WATERRUN:
    waterRun();
    break;
  case CHECKCANCEL:
    checkCancel();
    break;
  case ERROR:
    error();
    break;
  case DATA:
    dataReview();
    break;
  case SETTINGS:
    settingsChange();
    break;
  }
  if (mode != HOME && mode != ERROR) { //if arent on the home page, allow to go back
    back.drawButton(buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY, "Back", buttonColor, buttonTextColor);
    if (back.checkButton(buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
      back();
      background(backgroundColor);
      time = frameCount;
    }
  }
}

void serialEvent(Serial myPort) { //function for reading the serial data incoming
  try { //just in case exceptions are thrown
    // read a byte from the serial port:
    // if this is the first byte received, and it's an A, clear the serial
    // buffer and note that you've had first contact from the microcontroller.
    // Otherwise, add the incoming byte to the array:
    if (!connected) { //if havent established link yet
      int inByte = myPort.read();
      if (inByte == 'A') {
        println("Connected");
        connected = true; //established a link
        myPort.write('A');       //send back a confirm code
      }
    }else{ //otherwise its normal data retrieval
      // Add the latest byte from the serial port to array:
      int inByte = myPort.read();
      if (inByte == 'F') { //check fail code if occurs straight up
        reset();
        SDfailed = true; //cant run the program
        mode = ERROR; //return to home in case the SD card was removed
      }
      if (runStart) { //if are running normally collecting string data
        data = myPort.readStringUntil('d'); //read until end
        if (data != null) values = splitTokens(data, ",    ");
        influentVols = float(values); //convert string array to floats
        //printArray(influentVols); //for debugging
        //add the incoming data into the table csv
        if (influentVols[0] % saveTimeInterval == 0) { //if the time feeding in are of the right interval
          saveData(runName, table); //saves the data into a table entry
          // Set the points, the title and the axis labels
          dataPointCounter += saveTimeInterval; //onto another entry in the graphs array
          for (int i = 1; i <= numFunnels; i++) {
            points[i].add(dataPointCounter, influentVols[i]); //add a point to the graphs
            plot[i].setPoints(points[i]); //add points to the plot
          }
        } //end of save time interval
        byte[] buffer = new byte[10]; //read commands now
        myPort.readBytesUntil(13, buffer); //incoming buffer shoudl be clear of volume data
        //reads incoming data until the line feed
        int index;
        switch(buffer[0]) { //based on the incoming byte
          case 'D': //if a sample ahs been reached
            index = buffer[1]; //get the index value for which sample finished
            sample[index] = true; //this sample is now ready
            break;
          case 'L': //if received confirmation that sample is done
            index = buffer[1]; //get the index value for which sample finished
            if (!finished[index]) {
              funnelPointer ++; //count the done samples
              finished[index] = true; //this sample is now done
            }
            break;
          case 'F': //fail code in case SD has been removed while running
           reset(); //goto error mode
           SDfailed = true; //cant run the program
           mode = ERROR; //error mode
           break;
        }
      } //end of run start
    }//end of first contact
  } //end of try catch
   catch(RuntimeException e) { //in case exceptions are thrown by serial even function
    e.printStackTrace();
  }
}

void saveData(String fileName, Table saveTable) { //saves a new data entry into the tabel created for the run
  TableRow newRow = saveTable.addRow(); //add a row to the table and re save it
  newRow.setFloat("Time", influentVols[0]/60); //add the time and volume data time in min
  for (int i = 1; i <= numFunnels; i++) {
    newRow.setFloat("Funnel " + nf(i), influentVols[i]); //adda row to the tables
  }
  try{ //try to save to the file if tis available
    saveTable(saveTable, "data/" + fileName + ".csv"); //save the table as a file with name specified
  }catch(RuntimeException e) { //if the data file is busy it skips
    e.printStackTrace();
  }
}

//mode functions
void home() { //the main screen
  textSize(96); //draw title
  fill(textColor);
  text("Parallel System", width/2-indent*2.6, margin);
  timeAndDate(36, width/2-75, margin*2);
  if (connected && !SDfailed) { //if everything is normal draw the button normally
    start.drawButton(width/2, height/2, buttonSizeX, buttonSizeY, "Start", buttonColor, buttonTextColor);
  }else if (!connected && !SDfailed) { //if not connected and sd is okay the just gray out button
    start.drawButton(width/2, height/2, buttonSizeX, buttonSizeY, "Start", buttonColor-200, buttonTextColor); //draw the button funny if failed
  }
  if (start.checkButton(width/2, height/2, buttonSizeX, buttonSizeY) && !SDfailed && connected) { //draw and check if button is clicked
    if (needInput) { //if needed input do taht first
      emailPointer = 0; //start over
      mode = SETTINGS; //goto enter email mode
      emailInput = true;
      needInput = false; //no longer need to since we are doing it
    }else mode = SELECT; //otherwise go into select mode
    background(backgroundColor);
    time = frameCount;
  } 
}

void select() { //for choosing a mode
  textSize(96); //selction screen draw title
  fill(textColor);
  text("Select a Mode", width/2-indent*2.4, margin);
  timeAndDate(36, width/2-75, margin*2); //draw buttons and check
  purge.drawButton(width/2, height/2+buttonSizeY*1.5, buttonSizeX, buttonSizeY, "Purge", buttonColor, buttonTextColor);
  oneLiter.drawButton(width/2-buttonSizeX, height/2, buttonSizeX, buttonSizeY, "1 L Run", buttonColor, buttonTextColor);
  fourLiter.drawButton(width/2+buttonSizeX, height/2, buttonSizeX, buttonSizeY, "4 L Run", buttonColor, buttonTextColor);
  settings.drawButton(width-buttonSizeX, height-buttonSizeY, buttonSizeX, buttonSizeY, "Settings", buttonColor, buttonTextColor);
  if (purge.checkButton(width/2, height/2+buttonSizeY*1.5, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
    mode = PURGE;
    background(backgroundColor);
    time = frameCount;
  }
  if (oneLiter.checkButton(width/2-buttonSizeX, height/2, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
    mode = CHALLENGE;
    background(backgroundColor);
    time = frameCount;
  }
  if (fourLiter.checkButton(width/2+buttonSizeX, height/2, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
    mode = WATER;
    background(backgroundColor);
    time = frameCount;
  }
  if (settings.checkButton(width-buttonSizeX, height-buttonSizeY, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
    mode = SETTINGS;
    background(backgroundColor);
    time = frameCount;
  }
}

void keyPressed() { //when any kety is pressed use it to modify the text box
  if (mode == SETTINGS && emailInput && textBoxClicked) { //if in setting and inputting email
    text = textBox.modify(textLimit, text); //modify the textbox
  }
}

void settingsChange() { //allows the changiong of different settings like refreshing link and alos inputting email
  timeAndDate(36, width/2-75, margin*2); //draw time and date
  if (emailInput && !emailInputted) { //if inputting email
    textSize(96); //selction screen draw title
    fill(textColor);
    text("Enter Your Email", width/2-indent*2.4, margin); //draw buttons
    textSize(textSize); //write in the website
    text("@foliawater.com", width/2+indent*2.7, height/2+20);
    textBox.createTextBox(width/2-indent, height/2, textSize, textLimit); //create the text box
    //check if clicked on it, if so make it modifiable
    textButton.modifier = 1; //set modifier so that the button can be checked without being drawn
    //have to shift from then lower left corner of textbox drawing to center for button checking
    if (textButton.checkButton(width/2-indent, height/2, textBox.modifier, textSize) && frameCount - time > 10) {
      textBoxClicked = !textBoxClicked;
      time = frameCount; //store a debounce
    }
    if (textBoxClicked) textBox.drawIndicator(width/2-indent, height/2, textSize); //draw the texbox differnetly if its been clicked
    textBox.writeText(width/2-textBox.modifier/2-indent, height/2+20, textSize, text, textColor); //write text into it if available
    //do it after drawing text box so taht text goes on top of the box
    next.drawButton(width-buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY, "Next", buttonColor, buttonTextColor);
    if (next.checkButton(width-buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
      background(backgroundColor);
      time = frameCount;
      if (text != "") emailInputted = true; //if no text entered, cant advance
         //now inputted email once text was not blank and pressed next
    }
  }else if (emailInput && emailInputted) { //if an email has been entered
    textSize(96); //selction screen draw title
    fill(textColor);
    text("Confirm Your Email", width/2-indent*3.4, margin); //draw buttons
    textSize(textSize); //write in the website
    text("The email: ", width/2-indent*1.2, height/2-margin/2);
    fill(buttonTextColor);
    text(text + "@foliawater.com", width/2-indent*2.4, height/2);
    fill(textColor);
    text("will now recieve notifications from this system", width/2-indent*3.8, height/2+margin/2);
    next.drawButton(width-buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY, "Confirm", buttonColor, buttonTextColor);
    if (next.checkButton(width-buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
      background(backgroundColor);
      time = frameCount;
      reciever = text + "@foliawater.com"; //add typed text and the address to the reciever email
      emailInputted = false; //now have put in an email
      emailInput = false; //go back to regular settings
      text = ""; //reset the text
      JSONsave(); //save the new email in the JSON file
    }
  }else{ //if not inputting the email show selection screen
    textSize(96); //selction screen draw title
    fill(textColor);
    text("Settings", width/2-indent*1.2, margin); //draw buttons
    refresh.drawButton(width/2, height/2+buttonSizeY*1.5, buttonSizeX, buttonSizeY, "Refresh", buttonColor, buttonTextColor);
    if (refresh.checkButton(width/2, height/2+buttonSizeY*1.5, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
      background(backgroundColor);
      time = frameCount;
      refreshLink(width/2, height/2+buttonSizeY*1.5, buttonSizeX, buttonSizeY); //stops and restarts serial
    }
    email.drawButton(width/2, height/2, buttonSizeX, buttonSizeY, "Change Email", buttonColor, buttonTextColor);
    if (email.checkButton(width/2, height/2, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
      background(backgroundColor);
      time = frameCount;
      emailInput = true;
    }
  }
}

void JSONsave() { //reads exisiting json file and saves new entry with email recipient
   try{ //try to read a data file if its there
      emails = loadJSONArray("emails.json");
      parse = emails.getJSONObject(emails.size()-1); 
      emailPointer = parse.getInt("id");
      emailPointer++; //incrmeent to get a new entry
    }catch(Exception e) { //otherwise just print the error
      e.printStackTrace();
      emailPointer = 0; //start over
    }
    parse = new JSONObject();
    parse.setInt("id", emailPointer); //at new entry
    parse.setString("email", reciever); //save the email string
    emails.setJSONObject(emailPointer, parse); //save new object into array
    saveJSONArray(emails, "data/emails.json"); //save it into a file
}

void challenge() { //the 1 Liter run
  if (!funnelsSelected) { //if havent selected how many funnels to run
    selectFunnels(); //allow selection
  }else{ //now hav selected funnels
    textSize(72); //selection screen draw title
    fill(textColor);
    text("Ready to Run", width/2-200, margin);
    text(nf(numFunnels), width/2-200, margin+100); //display current number of funnels
    text("Funnels", width/2-150, margin+100);
    text("With Challenge Water", width/2-200, margin+200);
    textSize(textSize);
    text("Notifications will be", width/2-indent*2.6, margin+300);
    text("sent to: " + reciever, width/2-indent*2.6, margin+350);
    timeAndDate(48, width/2-indent*0.8, height/2+margin*2);
    start.drawButton(width/2, height/2, buttonSizeX, buttonSizeY, "Start", buttonColor, buttonTextColor);
    if (start.checkButton(width/2, height/2, buttonSizeX, buttonSizeY) && frameCount-time > 10) { //draw and check if button is clicked
      mode = CHALLENGERUN; ///begin the motors and the run
      background(backgroundColor);
      time = frameCount;
      reset(); //reset all vars quickly
      table.addColumn("Time", Table.FLOAT); //setup table
      for (int i = 1; i <= numFunnels; i++) { //create a table with the approriate columns
        table.addColumn("Funnel " + nf(i), Table.FLOAT);
      }
      plotSetup(1100); //setup for the graphs
      myPort.write('S'); //starts the run by sending the start command
      myPort.write(numFunnels); //to give the correct funnels selection
      delay(20); //wait for transmission then send over the start time data
      runName += " " + nf(month()) + "-" + nf(day()) + "-" + nf(year()) + " " + nf(hour()) + ".";
      if (minute() < 10) runName += "0" + nf(minute()); //add the zero to time values
      else runName += nf(minute());
      runName += (" 1 L Challenge Run "); //add some info to the SD card
      myPort.write(runName);
      myPort.write('E'); //for the end of the data string
      println(runName); //for debugging
      runStart = true; //now collect string data
    }
  }
}

void challengeRun() { //actually running the motors
  int spacing = width/(numFunnels+1);
  textSize(96);
  fill(textColor);
  text("1 L Challenge Run", width/2-indent*3, margin);
  noStroke(); //dont outline rectangles
  fill(backgroundColor); //fill to block out the background
  rect(width/2,height/2-margin*1.3, width, barSizeY*4); //draw a rectangle to clear previous texts
  stroke(0); //turn outlines back on in black as usual
  for (int i = 1; i <= numFunnels; i++) { //handles each funnel sequentially
    String label = " " + nf(i) + " "; //the labels are the numbers
    //need to convert data from serial input string to a progress bar
    try{ //just in case array is out of bounds or some other error
      progressBar(spacing*i, height/2-100, 0, 1000, influentVols[i]);
    }catch(RuntimeException e) {
      e.printStackTrace();
    }
    textSize(16);
    fill(textColor); //put label text in the right spot
    text("550",(spacing*i-barSizeX/2)+map(450,0,1000,0,barSizeX), height/2-indent);
    float xpos = spacing*i-barSizeX/2+map(550,0,1000,0,barSizeX); //the x pos of each line
    line(xpos, (height/2-100)-barSizeY/2, xpos, (height/2-100)+barSizeY/2);
    text("1000",(spacing*i-barSizeX/2)+map(850,0,1000,0,barSizeX), height/2-indent);
    //check if influent vol has eached the sample spot and signal
    //draw the buttons
    if (!sample[i]) { //if normal funnel no sampling, draw button normally
      funnel[i].drawButton(spacing*i, height/2, buttonSizeX, buttonSizeY, label, buttonColor, buttonTextColor);
    }else if (sample[i]) { //if are sampling flash button to indcate press
      if (frameCount % 20 == 0) flasher[i] = !flasher[i]; //toggle boolean to flash button
      if (!flasher[i]) funnel[i].drawButton(spacing*i, height/2, buttonSizeX, buttonSizeY, label, buttonColor, buttonTextColor);
      else funnel[i].drawButton(spacing*i, height/2, buttonSizeX, buttonSizeY, label, buttonColor-200, buttonTextColor);
      
      if (funnel[i].checkButton(spacing*i, height/2, buttonSizeX, buttonSizeY)) {//if button that was flashing was pressed
        background(backgroundColor);
        time = frameCount;
        myPort.write('C'); //clear sample code
        myPort.write(i); //send which button was pressed
        sample[i] = false; //this sample is now cleared
        hasTaken[i] = true; //and have taken this sample now
      }
    } //check to see if at the end
    if (finished[i]) { //if reached the end of the run
      funnel[i].drawButton(spacing*i, height/2, buttonSizeX, buttonSizeY, label, buttonColorGreen, buttonTextColor); //draw button in green
    }
  } //end of interated loop
  //add data mode functionality
  right.drawButton(width-buttonSizeX/4, height/2+buttonSizeY*1.4, buttonSizeX, buttonSizeY, ">", buttonColor, buttonTextColor);
  if (right.checkButton(width-buttonSizeX/4, height/2+buttonSizeY*1.4, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
    time = frameCount; //draw arrow for going into data mode
    background(backgroundColor);
    currentMode = CHALLENGERUN; //save the current mode since run is still going
    mode = DATA;
    maxPanels = numFunnels; //how many screens we can view
    panel = 0; //start on the first panel of overall data
  }
  //add pause button
  pause.drawButton(width/2, height/2+buttonSizeY*2, buttonSizeX, buttonSizeY, "Pause Run", buttonColor, buttonTextColor);
  if (pause.checkButton(width/2, height/2+buttonSizeY*2, buttonSizeX, buttonSizeY) && frameCount-time > 10 && !paused) {
    time = frameCount; //excutes a pause operation
    background(backgroundColor);
    paused = true; //are now paused
    pauseRun();
  }
  resume.drawButton(width/2, height/2+buttonSizeY*3.5, buttonSizeX, buttonSizeY, "Resume ", buttonColor, buttonTextColor);
  if (resume.checkButton(width/2, height/2+buttonSizeY*3.5, buttonSizeX, buttonSizeY) && frameCount-time > 10 && paused) {
    time = frameCount; //excutes a pause operation
    background(backgroundColor);
    currentMode = CHALLENGERUN; //save the mode currently
    resumeRun(); //resumes the run
    paused = false; //no longer paused
  }
  if (paused) { //if are paused, display that
    textSize(96);
    fill(textColor);
    text("Paused", width/2-indent, margin*2);
  }
  if (funnelPointer >= numFunnels) { //if all the samples are done, draw a next button
    next.drawButton(width-buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY, "Next", buttonColor, buttonTextColor);
    if (!lock) { //end of run onyl want to send stop once
      sendMail(reciever, runName, "Run has Finished");
      delay(1000); //wait for last data to come in
      myPort.write('X'); //to stop the run and prevent uncessary data collection
      lock = true;
    }
    if (next.checkButton(width-buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
      background(backgroundColor);
      time = frameCount;
      reset(); //reset the vars and return to home
      mode = HOME;
    }
  }
}

void water() { //the 4 liter run
  if (!funnelsSelected) { //if havent selected how many funnels to run
    selectFunnels(); //allow selection
  }else{ //if selected
    textSize(72); //selection screen draw title
    fill(textColor);
    text("Ready to Run", width/2-200, margin);
    text(nf(numFunnels), width/2-200, margin+100); //display current number of funnels
    text("Funnels", width/2-150, margin+100);
    text("With Pure Water", width/2-200, margin+200);
    textSize(textSize);
    text("Notifications will be", width/2-indent*2.6, margin+300);
    text("sent to: " + reciever, width/2-indent*2.6, margin+350);
    timeAndDate(48, width/2-indent*0.8, height/2+margin*2);
    start.drawButton(width/2, height/2, buttonSizeX, buttonSizeY, "Start", buttonColor, buttonTextColor);
    if (start.checkButton(width/2, height/2, buttonSizeX, buttonSizeY) && frameCount - time > 10) { //draw and check if button is clicked
      mode = WATERRUN; ///begin the motors and the run
      background(backgroundColor);
      time = frameCount;
      reset();
      table.addColumn("Time", Table.FLOAT); //setup table
      for (int i = 1; i <= numFunnels; i++) { //create a table with the approriate columns
        table.addColumn("Funnel " + nf(i), Table.FLOAT);
      }
      plotSetup(4010); //setup for the graphs ylim is 4000 ml
      myPort.write('S'); //starts the run by sending the start command
      myPort.write(numFunnels); //to give the correct funnels selection
      delay(20); //wait for transmission then send over the start time data
      //create a name for the run wiht the date and time
      runName += " " + nf(month()) + "-" + nf(day()) + "-" + nf(year()) + " " + nf(hour()) + ".";
      if (minute() < 10) runName += "0" + nf(minute()); //add the zero to time values
      else runName += nf(minute());
      runName += (" 4 L Run "); //add some info to the SD card
      myPort.write(runName);
      myPort.write('E'); //for the end of the data string
      println(runName); //for debugging
      runStart = true; //now collect string data
      delay(20); //send the code to bypass
      myPort.write('B'); //to bypass the sampling and stuff
    }
  }
}

void waterRun() { //actually running the water 
  int spacing = width/(numFunnels+1);
  textSize(96);
  fill(textColor);
  text("4 L Water Run", width/2-indent*2, margin);
  noStroke(); //dont outline rectangles
  fill(backgroundColor); //fill to block out the background
  rect(width/2,height/2-margin*1.3, width, barSizeY*4); //draw a rectangle to clear previous texts
  stroke(0); //turn outlines back on in black as usual
  for (int i = 1; i <= numFunnels; i++) { //draw all the buttons
    String label = " " + nf(i) + " "; //the labels are the numbers
    funnel[i].drawButton(spacing*i, height/2, buttonSizeX, buttonSizeY, label, buttonColor, buttonTextColor);
    try{ //just in case array out of bounds error or soemthing
      progressBar(spacing*i, height/2-100, 0, 4000, influentVols[i]); //progress bar based on 4 L
    }catch(RuntimeException e) {
      e.printStackTrace();
    }
    textSize(16);
    fill(textColor);
    text("4000",(spacing*i-barSizeX/2)+map(3200,0,4000,0,barSizeX), height/2-indent);
    if (finished[i]) { //if reached the end of the run
      funnel[i].drawButton(spacing*i, height/2, buttonSizeX, buttonSizeY, label, buttonColorGreen, buttonTextColor); //draw button in green
    }
    right.drawButton(width-buttonSizeX/4, height/2+buttonSizeY*1.4, buttonSizeX, buttonSizeY, ">", buttonColor, buttonTextColor);
    if (right.checkButton(width-buttonSizeX/4, height/2+buttonSizeY*1.4, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
      time = frameCount;
      background(backgroundColor);
      currentMode = WATERRUN; //save the current mode since run is still going
      mode = DATA;
      maxPanels = numFunnels; //how many screens we can view
      panel = 0; //start on the first panel of overall data
    }
    //add pause button
    pause.drawButton(width/2, height/2+buttonSizeY*2, buttonSizeX, buttonSizeY, "Pause Run", buttonColor, buttonTextColor);
    if (pause.checkButton(width/2, height/2+buttonSizeY*2, buttonSizeX, buttonSizeY) && frameCount-time > 10 && !paused) {
      time = frameCount; //excutes a pause operation
      background(backgroundColor);
      paused = true; //are now paused
      pauseRun();
    }
    resume.drawButton(width/2, height/2+buttonSizeY*3.5, buttonSizeX, buttonSizeY, "Resume ", buttonColor, buttonTextColor);
    if (resume.checkButton(width/2, height/2+buttonSizeY*3.5, buttonSizeX, buttonSizeY) && frameCount-time > 10 && paused) {
      time = frameCount; //excutes a pause operation
      background(backgroundColor);
      currentMode = WATERRUN; //save the mode currently
      resumeRun(); //resumes the run
      paused = false; //no longer paused
    }
    if (paused) { //if are paused, display that
      textSize(96);
      fill(textColor);
      text("Paused", width/2-indent, margin*2);
    }
  }
 if (funnelPointer >= numFunnels) { //if all the samples are done, draw a next button
    next.drawButton(width-buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY, "Next", buttonColor, buttonTextColor);
    if (!lock) { //end of run onyl want to send stop once
      sendMail(reciever, runName, "Run has Finished");
      delay(1000); //wait for last data to come in
      myPort.write('X'); //to stop the run and prevent uncessary data collection
      lock = true;
    }
    if (next.checkButton(width-buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
      time = frameCount;
      background(backgroundColor);
      reset(); //reset the vars and return to home
      mode = HOME;
    }
  }else{ //if havent finished yet, i.e not all funnels done 
    float timeElapsed = influentVols[0]/60; //gets the time elapsed since the start fo the run
    if (timeElapsed >= runTimeOut && !timeOut)  {
      timeOut = true; //have now timed out
      //send an email notification
      sendMail(reciever, runName, "Run Has been taking a long time (10 Hours), Is Everything Okay?");
    }
  }
}

void selectFunnels() {
  textSize(96); //selection screen draw title
  fill(textColor);
  text("Select How Many Funnels", width/2-indent*3.7, margin);
  text(nf(numFunnels), width/2, height/2); //display current number of funnels
  //draw buttons and check them
  up.drawButton(width/2+buttonSizeX, height/2-buttonSizeY/2, buttonSizeX/2, buttonSizeY, " ^ ", buttonColor, buttonTextColor);
  down.drawButton(width/2+buttonSizeX, height/2+buttonSizeY/2, buttonSizeX/2, buttonSizeY, " v ", buttonColor, buttonTextColor);
  if (up.checkButton(width/2+buttonSizeX, height/2-buttonSizeY/2, buttonSizeX/2, buttonSizeY) && frameCount-time > 10) {
    background(backgroundColor);
    numFunnels ++;
    time = frameCount; //increment num funnels with debounce delay
    if (numFunnels >= numFunnelsTotal) numFunnels = numFunnelsTotal; //constrain
  }
  if (down.checkButton(width/2+buttonSizeX, height/2+buttonSizeY/2, buttonSizeX/2, buttonSizeY) && frameCount-time > 10) {
    background(backgroundColor);
    numFunnels --;
    time = frameCount;
    if (numFunnels <= 1) numFunnels = 1; //constrain
  }
  next.drawButton(width-buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY, "Next", buttonColor, buttonTextColor);
  if (next.checkButton(width-buttonSizeX/2, height-buttonSizeY, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
    funnelsSelected = true;
    background(backgroundColor);
    time = frameCount;
  }
}

void purge() { //purges the lines with water
  if (!funnelsSelected) { //if havent selected how many funnels to run
    selectFunnels(); //allow selection
  }else{ //have selected already
    textSize(72); //selection screen draw title
    fill(textColor);
    text("Purging Lines on ", width/2-indent*1.7, margin);
    text(nf(numFunnels), width/2-indent*1.7, margin+100); //display current number of funnels
    text("Funnels", width/2-150, margin+100);
    //turns on motors for a bit then turns them off based on button control
    start.drawButton(width/2-buttonSizeX, height/2, buttonSizeX, buttonSizeY, "Start", buttonColor, buttonTextColor);
    stop.drawButton(width/2+buttonSizeX, height/2, buttonSizeX, buttonSizeY, "Stop", buttonColor, buttonTextColor);
    if (start.checkButton(width/2-buttonSizeX, height/2, buttonSizeX, buttonSizeY)) { //draw and check if button is clicked
      background(backgroundColor);
      time = frameCount;
      purging = true; //are now running
      purgeTimer = minute(); //get the current time
      myPort.write('P'); //starts the run by sending the start command
      myPort.write(numFunnels); //to give the correct funnels selection
    }
    if (stop.checkButton(width/2+buttonSizeX, height/2, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
      background(backgroundColor);
      time = frameCount;
      purging = false;
      myPort.write('X'); //stops the run by sending the stop command
    }
    if (minute() - purgeTimer >= purgeTimeLimit && purging) {
      myPort.write('X'); //stop the run if over time limit
      purging = false; //no longer purging
    }
  }
}

void refreshLink(float xPos, float yPos, float buttonSizeX, float buttonSizeY) { //refreshes connection to serial port
  try{ //in case it wasnt plugged in
    myPort.stop(); //closes and reopens serial port in case arduino was disconnected
  }catch(Exception e) {
    e.printStackTrace();
  }
  delay(100); //wait a bit to restart port
  connected = false; //no longer connected
  runStart = false; //no longer running
  serialSetup(); //restarts serial communication 
  time = 0; //set time var to 0 for anew count
  while (!connected) { //until get a response and are linked just darken button
    refresh.drawButton(xPos, yPos, buttonSizeX, buttonSizeY, "Refresh", buttonColor-200, buttonTextColor);
    //draw button darker, when arduino communicates it will connect
    time++; //increment the counter
    if (time > 50) break; //break out of while if stays too long
    delay(10); //give some delay in each loop
  }
  delay(200); // wait before sending commands
}

void progressBar(float x, float y, float minValue, float maxValue, float input) { //draws a progress bar based on input value
//positions x and y, sizes determined at top, color and fill color determined at top
//min and max are range of the input, input is the value tracked
  fill(barColor);
  rect(x,y, barSizeX, barSizeY); //draws bare bar
  fill(fillColor); //now fill it in appropriately based on min and max
  float progress = map(input, minValue, maxValue, 0, barSizeX); 
  rectMode(CORNER); //top get the rectangle at the right spot
  rect(x-barSizeX/2,y-barSizeY/2,progress, barSizeY);
  rectMode(CENTER); //change back to default progress bar
  float textXpos = ((x-barSizeX/2)+progress)-20;
  line((x-barSizeX/2)+progress, y-margin/1.6, (x-barSizeX/2)+progress, y-barSizeY/2);
  textSize(16); //place text to display current volumes
  fill(textColor);
  String currentVol = nfs(input, 0, 2); //get the current influent vol number truncate the decimal
  text(currentVol + " mL",textXpos, y-margin/1.5); //draw as text
}

void pauseRun() { //saves the paused run in a separate file
  pauses.clearRows(); //clear out the data table for the next pause
  pauses.addColumn("Name", Table.STRING); //add anme column
  TableRow pauseRow = pauses.addRow(); //add a row to the table for the name
  pauseRow.setString("Name", runName); 
  
  pauses.addColumn("Time", Table.FLOAT); //setup table header
  for (int i = 1; i <= numFunnels; i++) { //create a table with the approriate columns
    pauses.addColumn("Funnel " + nf(i), Table.FLOAT);
  }
  saveData("pauses", pauses); //save the pause table in a new file
  myPort.write('X'); //stop the run
}

void resumeRun() { //resumes a run from the saved file created when paused
  try{ //in case it wasnt plugged in
    myPort.stop(); //closes and reopens serial port in case arduino was disconnected
    delay(100); //wait a bit to restart port
    connected = false; //no longer connected
    runStart = false; //no longer running
    serialSetup(); //restarts serial communication
    time = 0; //refesh counter var
    while (!connected) { //until get a response and are linked just darken button
      resume.drawButton(width/2, height/2+buttonSizeY*3.5, buttonSizeX, buttonSizeY, "Resume ", buttonColor-200, buttonTextColor);
      //draw button darker, when arduino communicates it will connect
      time++; //increment the counter
      if (time > 50) break; //break out of while if stays too long
      delay(10); //give some delay in each loop
    }
    delay(200); // wait before sending commands
    myPort.write('R'); //starts the run by sending the start command top resume a run
    myPort.write(numFunnels); //to give the correct funnels selection
    delay(10); //wait for transmission then send over the start time data
    //need to write over the influent data which has been stored
    //update influent vols right before sending data to arduino
    pauses = loadTable("pauses.csv", "header");
    TableRow row = pauses.getRow(1);
    for (int i = 1; i <= numFunnels; i++) {
      influentVols[i] = row.getFloat("Funnel " + nf(i));
    }
    String data = ""; //declare local string for transmitting influent vols back
    data += nfs(influentVols[0], 0, 2) + ","; //send the 1st entry since thats the time
    for (int i = 1; i <= numFunnels; i++) {
      data += nf(influentVols[i]); //convert data to string with decimals
      data += ","; //add comma delimiters
    }
    myPort.write(data); //send over the string
    myPort.write('E'); //for the end of the data string
    //println(data); debug of whats sent to arduino
    runStart = true; //now collect string data
  }catch(Exception e) { //throw exception if still disconnected
    e.printStackTrace();
    mode = ERROR;
    runStart = true; //did stop from a run so tell that to error func
  }
} 

void back() { //handles going back
  switch (mode) { //based on which mode, goto the previous mode
  case HOME:
    mode = HOME;
    break;
  case SELECT:
    mode = HOME;
    break;
  case CHALLENGE:
    if (!funnelsSelected) mode = SELECT; //if wasnt selecting funnels
    else funnelsSelected = false; //if was, just go back
    break;
  case WATER:
    if (!funnelsSelected) mode = SELECT;
    else funnelsSelected = false;
    break;
  case PURGE:
    if (!funnelsSelected) mode = SELECT;
    else funnelsSelected = false;
    if (purging) { //only if was running
      purging = false; //are no longer purging
      myPort.write('X'); //automatically stop purging
    }
    break;
  case CHALLENGERUN:
    currentMode = mode; //store current mode while check cancel
    mode = CHECKCANCEL; //see if really want to go back
    break;
  case WATERRUN:
    currentMode = mode;
    mode = CHECKCANCEL;
    break;
  case DATA:
    mode = currentMode;
    break;
  case SETTINGS:
    if (!emailInput) mode = SELECT; //if not choosing an email go back
    else if(emailInput && !emailInputted) emailInput = false; //if are typing, just go back to settings
    else if (emailInput && emailInputted) emailInputted = false; //if confirmed, just go back to typing
    break;
  }
}

void checkCancel() { //are you sure you want to go back?
  textSize(72);
  fill(buttonTextColor);
  text("Are You Sure you want to go Back?", width/2-indent*4, margin);
  fill(textColor);
  text("Going back will end the current run", width/2-indent*4, margin*2);
  yes.drawButton(width/2-buttonSizeX, height/2, buttonSizeX, buttonSizeY, "Yes", buttonColor, buttonTextColor);
  no.drawButton(width/2+buttonSizeX, height/2, buttonSizeX, buttonSizeY, "No ", buttonColor, buttonTextColor);
  if (yes.checkButton(width/2-buttonSizeX, height/2, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
    reset(); //resets variables
    if (currentMode == CHALLENGERUN) mode = CHALLENGE;
    else if (currentMode == WATERRUN) mode = WATER;
    background(backgroundColor);
    time = frameCount; //increment num funnels with debounce delay
    myPort.write('X'); //send a couple X's to stop the run
  }
  if (no.checkButton(width/2+buttonSizeX, height/2, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
    background(backgroundColor);
    time = frameCount; //increment num funnels with debounce delay
    if (currentMode == CHALLENGERUN) mode = CHALLENGERUN;
    else if (currentMode == WATERRUN) mode = WATERRUN;
  }
}

void error() { //if something is wrong the program comes here
  if (!connected && !SDfailed) { //if not connected 
    //not physically connected
    fill(textColor);
    textSize(96);
    text("System not Connected", width/2-indent*3.5, height/2);
    settings.drawButton(width-buttonSizeX, height-buttonSizeY, buttonSizeX, buttonSizeY, "Settings", buttonColor, buttonTextColor);
    //allow settings to be reaches in this case
    if (settings.checkButton(width-buttonSizeX, height-buttonSizeY, buttonSizeX, buttonSizeY) && frameCount-time > 10) {
      mode = SETTINGS;
      background(backgroundColor);
      time = frameCount;
    }
    if (runStart) { //if stopped from a run then allow resuming
      paused = true; //run is paused again since it errored
      resume.drawButton(width/2, height/2+buttonSizeY*3.5, buttonSizeX, buttonSizeY, "Resume ", buttonColor, buttonTextColor);
      if (resume.checkButton(width/2, height/2+buttonSizeY*3.5, buttonSizeX, buttonSizeY) && frameCount-time > 10 && paused) {
        time = frameCount; //excutes a pause operation
        background(backgroundColor);
        resumeRun(); //resumes the run
        paused = false; //no longer paused if was paused
        mode = currentMode; //save the mode currently and restart from previous mode
      }
    }
  }else if (connected && SDfailed) { //if the SD card failed but are connected
    textSize(72); //nothign can do but power down
    fill(buttonColorRed);
    text("SD Card Failed", width/2-indent*2, height/2); //fail just the SD
  }
}

void dataReview() { //allows for real time viewing of the data as it is created
  left.drawButton(buttonSizeX/4, height/2+buttonSizeY*1.4, buttonSizeX, buttonSizeY, "<", buttonColor, buttonTextColor);
  if (panel < maxPanels) right.drawButton(width-buttonSizeX/4, height/2+buttonSizeY*1.4, buttonSizeX, buttonSizeY, ">", buttonColor, buttonTextColor);
  if (left.checkButton(buttonSizeX/4, height/2+buttonSizeY*1.4, buttonSizeX, buttonSizeY) && frameCount - time > 10) {
    time = frameCount;
    background(backgroundColor);
    panel--; //decrement the panels, if at last panel
    if (panel <= -1) mode = currentMode; //go back to the previous mode
  }
  if (right.checkButton(width-buttonSizeX/4, height/2+buttonSizeY*1.4, buttonSizeX, buttonSizeY) && frameCount - time > 10 && panel < maxPanels) {
    time = frameCount;
    background(backgroundColor);
    panel++; //increment the panels, button disappearing at the last one 
  }
  textSize(72); //write a tile for each panel
  fill(textColor); //if not on the start panel
  if (panel > 0) { //if looking at each funnel individually
    text("Data from Funnel: " + nf(panel), width/2-indent*2, margin);
    textSize(48); //print some data
    text("Influent Added: " + nfs(influentVols[panel], 0, 2) + " mL", width/2-indent*4, margin+indent);
    //draw each plot
    try { //try and plot in case it errors
      plot[panel].beginDraw();
      plot[panel].drawBox();
      plot[panel].drawXAxis();
      plot[panel].drawYAxis();
      plot[panel].drawTitle();
      plot[panel].drawPoints();
      plot[panel].drawLines();
      plot[panel].drawGridLines(GPlot.BOTH);
      plot[panel].endDraw();
    }catch (RuntimeException e) {
      e.printStackTrace();
    }
 
  }else{ //otherwise just at the first panel
    text("Run Statistics", width/2-indent*1.7, margin); //overall stats
    textSize(48);
    text("Name of Run: " + runName, width/2-indent*4, margin+indent); //print and truncate the numbers
    
    float timeElapsed = influentVols[0]/60; //get time elapsed in minutes
    String timeText; //for storing the string of the time
    if (timeElapsed >= 60) { // if large enough round down to display in hours:minutes format
      int hoursElapsed = floor(timeElapsed/60); //calculate time values
      float minutesElapsed = timeElapsed - 60*hoursElapsed; //calaculate the remaining minutes after the hour
      timeText = nf(hoursElapsed) + " hours" + nfs(minutesElapsed, 0, 2); //truncate decimals
      if (minutesElapsed == 1) timeText += " minute"; //plural vs singular
      else timeText += " minutes";
    }else{
      timeText = nfs(timeElapsed, 0, 2); //otherwise if less than 60 mins just print minutes
      if (timeElapsed == 1) timeText += " minute"; //plural vs singular
      else timeText += " minutes";
    }
    text("Time Elapsed: " + timeText, width/2-indent*4, margin+indent*2);
  }
}

void plotSetup(int ylim) { //function sets up the plots
  for (int i = 1; i <= numFunnels; i++) { //do the same for all plots
     plot[i].setPos(indent*1.5, margin*3); //set some parameters firstly location of top left
     plot[i].getTitle().setText("Funnel: " + nf(i)); //set titles
     plot[i].getYAxis().setAxisLabelText("Volume (mL)"); //set axes titles
     plot[i].getXAxis().setAxisLabelText("Time (minutes)");
     plot[i].setYLim(0, ylim);
     plot[i].setMar(60, 70, 40, 70); //set margins
     plot[i].setDim(800, 500); //size of the graph
     plot[i].setAxesOffset(4); //offset of the axes
     plot[i].setTicksLength(4); //tickmarkings
  }
}
//end mode functions

void reset() { //resets all the vars for a new run
  influentVols = new float[numFunnelsTotal+1]; //to refresh the array since it changes size
  for (int i = 1; i <= numFunnelsTotal; i++) {
    influentVols[i] = 0; //starts fresh
    sample[i] = false; //no samples
    flasher[i] = false;
    hasTaken[i] = false; //have not taken any samples yet
    finished[i] = false;  //not done
    points[i] = new GPointsArray(); //clear all points
    plot[i].setPoints(new GPointsArray());
    //clear arrays used to parse incoming data
  }
  runName = ""; //clear the run name
  dataPointCounter = 0; //reset back to 0 for the points
  funnelPointer = 0; //reset the done counter
  funnelsSelected = false; //havent sleected funnels
  runStart = false; //no longer running
  lock = false; //can end a run
  purging = false; //are not purging
  paused = false; //if go back no longer paused
  timeOut = false; //no longer timed out
  shouldResume = false;
  myPort.clear(); //clear the incoming data since stopped the run
}

void timeAndDate(int textSize, float xPos, float yPos) { //draws the time and date at the correct position
  int minutes = minute(); //retrieves the time from the system time
  int hours = hour();
  String timeOfDay = "am";
  int day = day();
  int dayOfWeek = calendar.get(Calendar.DAY_OF_WEEK);
  final String[] dayNames = { //some manipulations for the day of the week
    "Sunday", 
    "Monday", 
    "Tuesday", 
    "Wednesday", 
    "Thursday", 
    "Friday", 
    "Saturday"
  };
  String clause = "th"; //prefixes and months
  int month = month();
  final String[] monthNames = {
    "January", 
    "February", 
    "March", 
    "April", 
    "May", 
    "June", 
    "July", 
    "August", 
    "September", 
    "October", 
    "November", 
    "December"
  };
  int year = year();
  //before writing text, clears old text in case the time has changed
  noStroke(); //dont outline rectangles
  fill(backgroundColor); //fill to block out the background
  rect(xPos+margin-30,yPos+margin/3, textSize*7, textSize*4); //draw a rectangle to clear previous texts
  stroke(0); //turn outlines back on in black as usual
  
  textSize(textSize);
  fill(textColor);
  if (day == 1 || day == 21 || day == 31) { //the specifics of the clauses
    clause = "st";
  }
  else if (day == 2 || day == 22) {
    clause = "nd";
  }
  else if (day == 3 || day == 23) {
    clause = "rd";
  }
  else {
    clause = "th";
  }
  text(dayNames[dayOfWeek-1], xPos+textSize/2.7-30, yPos); //write the day then the month
  text(monthNames[month-1] + " " + day + clause + " " + year, xPos-textSize*1.3, yPos+textSize);
  if (hours >= 12) { //speicifcs to convert 24 h time to 12 h time
    timeOfDay = "pm"; 
    if (hours > 12) hours -= 12;
  }
  else if (hours < 12) {
    timeOfDay = "am";
    if (hours == 0) hours = 12;
  }
  if (minutes < 10) text(hours + ":0" + minutes + timeOfDay, xPos, yPos+2*textSize); 
    //adds the appropriate zeroes for the time, 12:09 for ex
  else text(hours + ":" + minutes + timeOfDay, xPos, yPos+2*textSize);
}
