
//Open Source License.
//
//Copyright 2019 Ecole Polytechnique Federale Lausanne (EPFL)
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in
//all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//THE SOFTWARE.


package main

import (
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"os/user"
	"path/filepath"

	"github.com/pkg/sftp"
	"golang.org/x/crypto/ssh"
        "golang.org/x/crypto/ssh/terminal"
)

func GetSigner(pemBytes []byte) (ssh.Signer, error) {
	signerwithoutpassphrase, err := ssh.ParsePrivateKey(pemBytes)
	if err != nil {
		fmt.Print("SSH Key Passphrase [none]: ")
		passPhrase, err := terminal.ReadPassword(0)
		if err != nil {
			return nil, err
		}
		signerwithpassphrase, err := ssh.ParsePrivateKeyWithPassphrase(pemBytes, passPhrase)
		if err != nil {
			return nil, err
		} else {
			return signerwithpassphrase, err
		}
	} else {
		return signerwithoutpassphrase, err
	}
}

func publicKeyFile(file string) (ssh.AuthMethod, error) {
	buffer, err := ioutil.ReadFile(file)
	if err != nil {
		return nil, fmt.Errorf("Error reading key file %s\n", err)
	}
	//key, err := ssh.ParsePrivateKey(buffer)
        key, err := GetSigner(buffer)
	if err != nil {
		return nil, err
	}
	return ssh.PublicKeys(key), nil
}

func createSession(connection *ssh.Client) (*ssh.Session, error) {
	session, err := connection.NewSession()
	if err != nil {
		return nil, fmt.Errorf("Failed to create session: %s", err)
	}
	modes := ssh.TerminalModes{
		// ssh.ECHO:          0,     // disable echoing
		ssh.TTY_OP_ISPEED: 14400, // input speed = 14.4kbaud
		ssh.TTY_OP_OSPEED: 14400, // output speed = 14.4kbaud
	}

	if err := session.RequestPty("xterm", 80, 40, modes); err != nil {
		session.Close()
		return nil, fmt.Errorf("Error in pty")
	}

	err = configIO(session)
	if err != nil {
		return nil, err
	}
	return session, nil
}

func configIO(session *ssh.Session) error {
	/*
		stdin, err := session.StdinPipe()
		if err != nil {
			return fmt.Errorf("Unable to setup stdin for session: %v", err)
		}
		go io.Copy(stdin, os.Stdin)
	*/
	stdout, err := session.StdoutPipe()
	if err != nil {
		return fmt.Errorf("Unable to setup stdout for session: %v", err)
	}
	go io.Copy(os.Stdout, stdout)

	stderr, err := session.StderrPipe()
	if err != nil {
		return fmt.Errorf("Unable to setup stderr for session: %v", err)
	}
	go io.Copy(os.Stderr, stderr)
	return nil
}

func remoteCopy(client *ssh.Client, agentPath string) error {
	// open an SFTP session over an existing ssh connection.
	sftp, err := sftp.NewClient(client)
	if err != nil {
		return fmt.Errorf("Can't crete sftp client: %s", err)
	}
	defer sftp.Close()

	currentUser, _ := user.Current()
	dstPath := fmt.Sprintf("/tmp/%s/%s", currentUser.Username,
		filepath.Base(agentPath))

	// Create remote folder if doesn't exist
	sftp.Mkdir(fmt.Sprintf("/tmp/%s", currentUser.Username))

	// Create the remote file
	dstFile, err := sftp.Create(dstPath)
	if err != nil {
		fmt.Println("destFile ", dstFile, " and dest path ", dstPath, "agentPath ", agentPath)
		return fmt.Errorf("Error creating remote file: %s\n", err)
	}
	defer dstFile.Close()

	// Open local file
	buffer, err := ioutil.ReadFile(agentPath)
	if err != nil {
		return fmt.Errorf("Error local path: %s", err)
	}

	// Copy the file
	dstFile.Write(buffer)

	// Give permissions
	err = sftp.Chmod(dstPath, os.ModePerm)
	if err != nil {
		return err
	}
	return nil
}

func deployAgent(dst, execPath, args string) (*ssh.Session, error) {
	currentUser, _ := user.Current()
	keyFile, err := publicKeyFile(fmt.Sprintf("/home/%s/.ssh/id_rsa_lancet",
		currentUser.Username))
	if err != nil {
		return nil, err
	}

	sshConfig := &ssh.ClientConfig{
		User:            currentUser.Username,
		Auth:            []ssh.AuthMethod{keyFile},
		HostKeyCallback: ssh.InsecureIgnoreHostKey(),
	}

	// Copy remote
	connection, err := ssh.Dial("tcp", fmt.Sprintf("%v:22", dst), sshConfig)
	if err != nil {
		return nil, fmt.Errorf("Failed to dial: %s", err)
	}
	err = remoteCopy(connection, execPath)
	if err != nil {
		return nil, err
	}
	session, err := createSession(connection)
	if err != nil {
		return nil, err
	}

	cmd := fmt.Sprintf("ulimit -c unlimited && sudo /tmp/%s/%s %s", currentUser.Username,
		filepath.Base(execPath), args)
	go session.Run(cmd)
	return session, nil
}

func start_vms(dst, args string) (*ssh.Session, error) {
	currentUser, _ := user.Current()
	keyFile, err := publicKeyFile(fmt.Sprintf("/home/%s/.ssh/id_rsa_lancet",
		currentUser.Username))
	if err != nil {
		return nil, err
	}

	sshConfig := &ssh.ClientConfig{
		User:            currentUser.Username,
		Auth:            []ssh.AuthMethod{keyFile},
		HostKeyCallback: ssh.InsecureIgnoreHostKey(),
	}

	// Copy remote
	connection, err := ssh.Dial("tcp", fmt.Sprintf("%v:22", dst), sshConfig)
	if err != nil {
		return nil, fmt.Errorf("Failed to dial: %s", err)
	}
	session, err := createSession(connection)
	if err != nil {
		return nil, err
	}

	cmd := fmt.Sprintf("/home/ustiugov/faas_study/helper_scripts/9.cleanup.sh && " +
                "/home/ustiugov/faas_study/setup_run_fc.sh %s", args)
	go session.Run(cmd)
	return session, nil
}
