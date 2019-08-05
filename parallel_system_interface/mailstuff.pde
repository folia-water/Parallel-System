// Example functions that send mail (smtp)
// You can also do imap, but that's not included here
 
// A function to check a mail account
import java.util.*;
import java.io.*;
import javax.activation.*;
 
// A function to send mail
void sendMail(String recipient, String subject, String body) {
  // Create a session
  String host="smtp.gmail.com";
  Properties props=new Properties();
 
  // SMTP Session
  props.put("mail.transport.protocol", "smtp");
  props.put("mail.smtp.host", host);
  props.put("mail.smtp.port", "587");
  props.put("mail.smtp.auth", "true");
  // We need TTLS, which gmail requires
  props.put("mail.smtp.starttls.enable","true");
 
  // Create a session
  Session session = Session.getDefaultInstance(props, new Auth());
 
  try
  {
    MimeMessage msg=new MimeMessage(session);
    msg.setFrom(new InternetAddress("avbu8sdqin@foliawater.com", "Parallel System"));
    msg.addRecipient(Message.RecipientType.TO,new InternetAddress(recipient));
    msg.setSubject(subject);
    BodyPart messageBodyPart = new MimeBodyPart();
 // Fill the message
    messageBodyPart.setText(body);
    Multipart multipart = new MimeMultipart();
    multipart.addBodyPart(messageBodyPart);
    msg.setContent(multipart);
    msg.setSentDate(new Date());
    Transport.send(msg);
    println("Mail sent!");
  }
  catch(Exception e)
  {
    e.printStackTrace();
  }
 
}
