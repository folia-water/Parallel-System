// Simple Authenticator      
// Careful, this is terribly unsecure!!

//uses parallel system dedicated email
//user: avbu8sdqin@foliawater.com
//pass: vNh3Tx69HrryTki
 
import javax.mail.Authenticator;
import javax.mail.PasswordAuthentication;
 
public class Auth extends Authenticator {
 
  public Auth() {
    super();
  }
 
  public PasswordAuthentication getPasswordAuthentication() {
    String username, password;
    username = "avbu8sdqin@foliawater.com";
    password = "vNh3Tx69HrryTki";
    System.out.println("authenticating... ");
    return new PasswordAuthentication(username, password);
  }
}
