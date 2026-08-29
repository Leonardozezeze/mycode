use rand::Rng;
use std::cmp::Ordering;
use std::io;
fn main() {
    println!("Guess Nunmber Game!");
    let secret_number = rand::thread_rng().gen_range(1, 101);
    loop {
        println!("Guess a number:");
        let mut guess = String::new();
        io::stdin().read_line(&mut guess).expect("无法读取行");
        let guess: u32 =match guess.trim().parse(){
            Ok(num)=>num,
            Err(_)=>continue,
        };
        println!("The number you've guessed is:{}", guess);
        match guess.cmp(&secret_number) {
            Ordering::Less => println!("Too small!"),
            Ordering::Equal => {println!("You win!");break;},
            Ordering::Greater => println!("Too big!"),
        }
    }
}
